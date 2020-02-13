/********************************************************************************
* Copyright (c) 2010-2019 Robert Bosch GmbH
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License 2.0 which is available at
* http://www.eclipse.org/legal/epl-2.0.
*
* SPDX-License-Identifier: EPL-2.0
*
* Contributors:
*    Robert Bosch GmbH - initial contribution
*
********************************************************************************/

/**
 * @file
 *
 * @brief The engine of the Cellular driver.
 */

#include "Kiso_CellularModules.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ENGINE

#include "Engine.h"

#include "AtUrc.h"
#include "Hardware.h"

#include "Kiso_Basics.h"
#include "Kiso_Retcode.h"
#include "Kiso_Assert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "portmacro.h"

#include "Kiso_Logging.h"

/**
 * @brief Number of milliseconds after which to time-out a tx transfer on the
 * serial interface.
 */
#define CELLULAR_SEND_AT_COMMAND_WAIT_TIME (UINT32_C(1000) / portTICK_PERIOD_MS)

static StaticSemaphore_t RxWakeupBuffer;        //!< Semaphore storage for rx data ready signalling.
static SemaphoreHandle_t RxWakeupSignal = NULL; //!< Handle for rx data ready semaphore.

static StaticSemaphore_t TxWakeupBuffer;        //!< Semaphore storage for tx data sent signalling.
static SemaphoreHandle_t TxWakeupSignal = NULL; //!< Handle for tx data sent semaphore.

static StaticTask_t IdleUrcListenerTaskBuffer;                             //!< Static task allocation for Idle-URC-Listener task.
static StackType_t IdleUrcListenerTaskStack[CELLULAR_DRV_TASK_STACK_SIZE]; //!< Stack for Idle-URC-Listener task.
static TaskHandle_t IdleUrcListenerTask = NULL;                            //!< Handle of the Idle-URC-Listener task.

static UART_T Serial = (UART_T)NULL; //!< Handle to the MCU Essentials serial device.

static Cellular_State_T State = CELLULAR_STATE_POWEROFF; //!< Current driver state.
static Cellular_StateChanged_T OnStateChanged = NULL;    //!< Callback to user-code to notify about driver events.

static bool IsEchoModeEnabled = true; //!< State of AT echo mode (on/off)

static uint8_t TransceiverRxBuffer[CELLULAR_RX_BUFFER_SIZE]; //!< Rx-buffer used for the shared #AtTransceiver_S instance.
static struct AtTransceiver_S Transceiver;
static uint8_t UartRxByte; //!< single byte rx buffer

static Retcode_T WriteOntoSerial(const void *data, size_t length, size_t *numBytesWritten)
{
    Retcode_T rc;
    if (NULL == data)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if ((UART_T)NULL == Serial)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }

    /* ensure the semaphore is NOT signaled before we begin to send */
    (void)xSemaphoreTake(TxWakeupSignal, 0);

#if CELLULAR_ENABLE_TRACING
    LOG_DEBUG("len:%zu >%.*s", length, (int)length, (const char *)data);
#endif
    rc = MCU_UART_Send(Serial, (const uint8_t *)data, length);
    if (RETCODE_OK != rc)
    {
        return rc;
    }

    /* wait the end of serial transfer */
    BaseType_t result = xSemaphoreTake(TxWakeupSignal, CELLULAR_SEND_AT_COMMAND_WAIT_TIME);
    if (pdPASS != result)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
    }

    if (numBytesWritten)
    {
        *numBytesWritten = length;
    }

    return rc;
}

static void HandleMcuIsrCallback(UART_T uart, struct MCU_UART_Event_S event)
{
    KISO_UNUSED(uart);
    assert(uart != 0);

    if (event.TxComplete)
    {
        /* all bytes have been transmitted, signal semaphore */
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        (void)xSemaphoreGiveFromISR(TxWakeupSignal, &higherPriorityTaskWoken); //LCOV_EXCL_BR_LINE
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }

    if (event.RxComplete)
    {
        Retcode_T rc = AtTransceiver_Feed(&Transceiver, &UartRxByte, sizeof(UartRxByte), NULL); //LCOV_EXCL_BR_LINE
        if (RETCODE_OK != rc)
        {
            Retcode_RaiseErrorFromIsr(rc); //LCOV_EXCL_BR_LINE
            return;
        }

        // if (AT_DEFAULT_S4_CHARACTER == UartRxByte)
        // {
        /* wakeup idle URC-listener */
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        (void)xSemaphoreGiveFromISR(RxWakeupSignal, &higherPriorityTaskWoken); //LCOV_EXCL_BR_LINE
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
        // }
    }
}

static void ListenForUrcDuringIdle(void)
{
    /* Wait for the rx wakeup signal, indicating the arival of at least one S4
     * character. */
    (void)xSemaphoreTake(RxWakeupSignal, portMAX_DELAY);

    /* Looks like we got some data. If the bytes are not meant for us, the
     * command-sender will have already locked the transceiver at this time. If
     * not, we take ownership of the transceiver and interpret whatever is in
     * there as URC. */
    Retcode_T rc = AtTransceiver_Lock(&Transceiver);
    if (RETCODE_OK != rc)
    {
        Retcode_RaiseError(rc);
        return;
    }
    LOG_DEBUG("Handling URCs while idleing."); //LCOV_EXCL_BR_LINE

    rc = Urc_HandleResponses();
    if (RETCODE_OK != rc)
    {
        Retcode_RaiseError(rc);
    }

    rc = AtTransceiver_Unlock(&Transceiver);
    if (RETCODE_OK != rc)
    {
        Retcode_RaiseError(rc);
        return;
    }
}

//LCOV_EXCL_START
static void RunIdleUrcListener(void *param)
{
    KISO_UNUSED(param);

    while (1)
    {
        ListenForUrcDuringIdle();
    }
}
//LCOV_EXCL_STOP

Retcode_T Engine_Initialize(Cellular_StateChanged_T onStateChanged)
{
    if (NULL == onStateChanged)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    Retcode_T status;

    LOG_DEBUG("Initializing Cellular engine.");

    RxWakeupSignal = xSemaphoreCreateBinaryStatic(&RxWakeupBuffer);
    assert(NULL != RxWakeupSignal); /* due to static allocation */

    TxWakeupSignal = xSemaphoreCreateBinaryStatic(&TxWakeupBuffer);
    assert(NULL != TxWakeupSignal); /* due to static allocation */

    status = Hardware_Initialize(HandleMcuIsrCallback, &UartRxByte);
    if (RETCODE_OK != status)
    {
        LOG_FATAL("Failed to initialize Hardware!"); //LCOV_EXCL_BR_LINE
        return status;
    }

    status = Hardware_GetCommunicationChannel(&Serial);
    if (RETCODE_OK != status)
    {
        LOG_FATAL("Failed to obtain communications-channel!"); //LCOV_EXCL_BR_LINE
        return status;
    }

    status = AtTransceiver_Initialize(&Transceiver, TransceiverRxBuffer, CELLULAR_RX_BUFFER_SIZE, WriteOntoSerial);

    IdleUrcListenerTask = xTaskCreateStatic(RunIdleUrcListener, "IdleUrcListener", CELLULAR_DRV_TASK_STACK_SIZE, NULL, CELLULAR_DRV_TASK_PRIORITY, IdleUrcListenerTaskStack, &IdleUrcListenerTaskBuffer);
    assert(NULL != IdleUrcListenerTask); /* due to static allocation */

    OnStateChanged = onStateChanged;
    State = CELLULAR_STATE_POWEROFF;
    IsEchoModeEnabled = true;

    return RETCODE_OK;
}

void Engine_NotifyNewState(Cellular_State_T newState, void *param, uint32_t len)
{
    assert(NULL != OnStateChanged);
    if (newState != State)
    {
        Cellular_State_T oldState = State;
        State = newState;

        OnStateChanged(oldState, newState, param, len);
    }
}

void Engine_SetEchoMode(bool echoMode)
{
    IsEchoModeEnabled = echoMode;
}

bool Engine_GetEchoMode(void)
{
    return IsEchoModeEnabled;
}

Retcode_T Engine_OpenTransceiver(struct AtTransceiver_S **atTransceiver)
{
    Retcode_T rc = RETCODE_OK;
    if (NULL == atTransceiver)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
        goto early_error;
    }
    rc = AtTransceiver_Lock(&Transceiver);
    if (RETCODE_OK != rc)
    {
        goto early_error;
    }

    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    options |= ATTRANSCEIVER_WRITEOPTION_NOBUFFER;
    if (!IsEchoModeEnabled)
    {
        options |= ATTRANSCEIVER_WRITEOPTION_NOECHO;
    }
    rc = AtTransceiver_PrepareWrite(&Transceiver, options, NULL, 0);
    if (RETCODE_OK != rc)
    {
        goto late_error;
    }
    *atTransceiver = &Transceiver;

    return RETCODE_OK;

early_error:
    return rc;

late_error:
    (void)AtTransceiver_Unlock(&Transceiver);
    return rc;
}

Retcode_T Engine_CloseTransceiver(void)
{
    return AtTransceiver_Unlock(&Transceiver);
}

void Engine_Deinitialize(void)
{
    assert(NULL != RxWakeupSignal &&
           NULL != TxWakeupSignal &&
           NULL != IdleUrcListenerTask &&
           NULL != OnStateChanged);

    vTaskDelete(IdleUrcListenerTask);
    IdleUrcListenerTask = NULL;

    Retcode_T rc = Hardware_Deinitialize();
    if (RETCODE_OK != rc)
    {
        LOG_FATAL("Failed to deinitialize Hardware!"); //LCOV_EXCL_BR_LINE
        Retcode_RaiseError(rc);
    }

    Serial = (UART_T)0;

    vSemaphoreDelete(RxWakeupSignal);
    RxWakeupSignal = NULL;

    vSemaphoreDelete(TxWakeupSignal);
    TxWakeupSignal = NULL;

    OnStateChanged = NULL;
}
