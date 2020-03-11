/*******************************************************************************
 * Copyright (c) 2010-2020 Robert Bosch GmbH
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
 ******************************************************************************/

#include "AppInfo.h"
#define KISO_MODULE_ID APP_MODULE_ID_ETHERNET

#include "Ethernet.h"

#include "BSP_NucleoF767.h"

#include "Kiso_Basics.h"
#include "Kiso_CmdProcessor.h"
#include "Kiso_Logging.h"
#include "Kiso_BSP_LED.h"
#include "Kiso_BSP_Ethernet.h"
#include "Kiso_MCU_Ethernet.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include <stdio.h>

#define ETHERNET_SELFINDUCEDTRANSMIT_TICKPERIOD (pdMS_TO_TICKS(5000U))

#define ETHERNET_LED_TICKPERIOD (pdMS_TO_TICKS(100U))
#define ETHERNET_LED_TRANSMIT_LEDONTICKS (pdMS_TO_TICKS(100U) / ETHERNET_LED_TICKPERIOD)
#define ETHERNET_LED_RECEIVE_LEDONTICKS (pdMS_TO_TICKS(100U) / ETHERNET_LED_TICKPERIOD)
#define ETHERNET_LED_ERROR_LEDONTICKS (pdMS_TO_TICKS(500U) / ETHERNET_LED_TICKPERIOD)

#define ETHERNET_TIMEOUT_TRANSMIT (pdMS_TO_TICKS(100U))

#define ETHERNET_RXPOOL_LENGTH (1U)
#define ETHERNET_RXPOOLBUFFER_SIZE (1514U)

static inline char HexCharFromNibble(uint8_t quintetIndex, uint8_t byte);
static inline void BinToHexMacAddress(const void *bin, size_t binLength,
                                      char *hex, size_t hexLength);
static void IndicateSignal(void *param1, uint32_t param2);
static void HandleEthernetEvent(Ethernet_T eth, struct MCU_Ethernet_Event_S event);
static void HandleLedTimerTick(TimerHandle_t timer);
static void HandleSelfInducedTransmitTick(TimerHandle_t timer);
static void ReceiveFrame(void *param1, uint32_t param2);
static void TransmitFrame(void *param1, uint32_t param2);

static CmdProcessor_T *AppCmdProcessor;
static Ethernet_T Ethernet = NULL;
static TimerHandle_t LedTimer;
static TimerHandle_t SelfInducedTransmitTimer;
static SemaphoreHandle_t TransmitCompleteSignal;

static char RxPoolBuffers[ETHERNET_RXPOOL_LENGTH][ETHERNET_RXPOOLBUFFER_SIZE];
static struct MCU_Ethernet_FrameBuffer_S RxPool[ETHERNET_RXPOOL_LENGTH];

static const uint8_t DstMacAddress[MCU_ETHERNET_MACLENGTH] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // ### REPLACE THIS WITH YOUR PC's MAC ADDRESS ###
static const uint8_t SrcMacAddress[MCU_ETHERNET_MACLENGTH] = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef};

struct SignalLed_S
{
    bool Scheduled;
    TickType_t TickCounter;
    TickType_t TurnOffTicks;
    uint32_t LedId;
};

struct SignalLed_S TransmitSignal = {.Scheduled = false,
                                     .TickCounter = 0U,
                                     .TurnOffTicks = ETHERNET_LED_TRANSMIT_LEDONTICKS,
                                     .LedId = NUCLEOF767_LED_BLUE_ID};
struct SignalLed_S ReceiveSignal = {.Scheduled = false,
                                    .TickCounter = 0U,
                                    .TurnOffTicks = ETHERNET_LED_RECEIVE_LEDONTICKS,
                                    .LedId = NUCLEOF767_LED_GREEN_ID};
struct SignalLed_S ErrorSignal = {.Scheduled = false,
                                  .TickCounter = 0U,
                                  .TurnOffTicks = ETHERNET_LED_ERROR_LEDONTICKS,
                                  .LedId = NUCLEOF767_LED_RED_ID};

struct SignalLed_S *const Signals[3] = {
    &TransmitSignal,
    &ReceiveSignal,
    &ErrorSignal,
};

static inline char HexCharFromNibble(uint8_t quintetIndex, uint8_t byte)
{
    const char *hexChars = "0123456789ABCDEF";
    uint8_t mask = (0 == quintetIndex) ? 0x0F : 0xF0;
    uint8_t shift = (0 == quintetIndex) ? 0 : 4;
    uint8_t quintet = (byte & mask) >> shift;

    return hexChars[quintet];
}

static inline void BinToHexMacAddress(const void *bin, size_t binLength,
                                      char *hex, size_t hexLength)
{
    assert(hexLength >= binLength * 3);
    for (size_t i = 0, j = 0; i < binLength; ++i, j += 3)
    {
        hex[j + 0] = HexCharFromNibble(1, ((const uint8_t *)bin)[i]);
        hex[j + 1] = HexCharFromNibble(0, ((const uint8_t *)bin)[i]);
        hex[j + 2] = ':';
    }
    hex[hexLength - 1] = '\0';
}

static void IndicateSignal(void *param1, uint32_t param2)
{
    KISO_UNUSED(param2);

    struct SignalLed_S *signal = (struct SignalLed_S *)param1;

    Retcode_T rc = BSP_LED_Switch(signal->LedId, NUCLEOF767_LED_COMMAND_ON);
    if (RETCODE_OK != rc)
    {
        Retcode_RaiseError(rc);
        return;
    }

    signal->Scheduled = true;
    signal->TickCounter = 0;
}

static void HandleEthernetEvent(Ethernet_T eth, struct MCU_Ethernet_Event_S event)
{
    KISO_UNUSED(eth);
    KISO_UNUSED(event);

    if (event.TxStopped)
    {
        if (event.IsIsr)
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            (void)xSemaphoreGiveFromISR(TransmitCompleteSignal, &higherPrioTaskWoken);
            portYIELD_FROM_ISR(higherPrioTaskWoken);

            (void)CmdProcessor_EnqueueFromIsr(AppCmdProcessor, IndicateSignal, &TransmitSignal, 0U);
        }
        else
        {
            (void)xSemaphoreGive(TransmitCompleteSignal);

            (void)CmdProcessor_Enqueue(AppCmdProcessor, IndicateSignal, &TransmitSignal, 0U);
        }
    }

    if (event.RxAvailable)
    {
        if (event.IsIsr)
        {
            (void)CmdProcessor_EnqueueFromIsr(AppCmdProcessor, ReceiveFrame, NULL, 0U);
            (void)CmdProcessor_EnqueueFromIsr(AppCmdProcessor, IndicateSignal, &ReceiveSignal, 0U);
        }
        else
        {
            (void)CmdProcessor_Enqueue(AppCmdProcessor, ReceiveFrame, NULL, 0U);
            (void)CmdProcessor_Enqueue(AppCmdProcessor, IndicateSignal, &ReceiveSignal, 0U);
        }
    }

    if (event.RxError || event.TxError)
    {
        if (event.IsIsr)
            (void)CmdProcessor_EnqueueFromIsr(AppCmdProcessor, IndicateSignal, &ErrorSignal, 0U);
        else
            (void)CmdProcessor_Enqueue(AppCmdProcessor, IndicateSignal, &ErrorSignal, 0U);
    }
}

static void ReceiveFrame(void *param1, uint32_t param2)
{
    KISO_UNUSED(param1);
    KISO_UNUSED(param2);

    struct MCU_Ethernet_FrameBuffer_S *rxFrame = NULL;
    Retcode_T rc = MCU_Ethernet_GetAvailableRxFrame(Ethernet, &rxFrame);

    if (RETCODE_OK == rc)
    {
        assert(NULL != rxFrame);

        char dst[MCU_ETHERNET_MACLENGTH * 3];
        char src[MCU_ETHERNET_MACLENGTH * 3];
        BinToHexMacAddress((uint8_t *)rxFrame->Data + 0, MCU_ETHERNET_MACLENGTH, dst, sizeof(dst));
        BinToHexMacAddress((uint8_t *)rxFrame->Data + MCU_ETHERNET_MACLENGTH, MCU_ETHERNET_MACLENGTH, src, sizeof(src));

        LOG_INFO("Received %zu-byte frame, dst: %s, src: %s", rxFrame->Length, dst, src);
        LOG_INFO("> %.*s", rxFrame->Length - 14U, (uint8_t *)rxFrame->Data + 14U);

        Retcode_T finallyRc = MCU_Ethernet_ReturnToRxPool(Ethernet, rxFrame);
        if (RETCODE_OK != finallyRc && RETCODE_OK == rc)
        {
            rc = finallyRc;
        }
    }

    if (RETCODE_OK != rc)
    {
        Retcode_RaiseError(rc);
    }
}

static void TransmitFrame(void *param1, uint32_t param2)
{
    KISO_UNUSED(param1);
    KISO_UNUSED(param2);

    LOG_DEBUG("Transmitting frame...");

    MCU_Ethernet_ResetNextFrame(Ethernet);
    Retcode_T rc = MCU_Ethernet_AppendToNextFrame(Ethernet, DstMacAddress, MCU_ETHERNET_MACLENGTH);

    if (RETCODE_OK == rc)
    {
        rc = MCU_Ethernet_AppendToNextFrame(Ethernet, SrcMacAddress, MCU_ETHERNET_MACLENGTH);
    }

    const uint8_t typeAndPayload[] = {0x00, 0x0B,
                                      0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64};
    size_t length = sizeof(typeAndPayload);

    if (RETCODE_OK == rc)
    {
        rc = MCU_Ethernet_AppendToNextFrame(Ethernet, typeAndPayload, length);
    }

    if (RETCODE_OK == rc)
    {
        rc = MCU_Ethernet_TransmitFrame(Ethernet);
    }

    if (RETCODE_OK == rc)
    {
        BaseType_t taken = xSemaphoreTake(TransmitCompleteSignal, ETHERNET_TIMEOUT_TRANSMIT);
        if (taken)
        {
            LOG_DEBUG("Transmit complete.");
        }
        else
        {
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
        }
    }

    if (RETCODE_OK != rc)
    {
        LOG_ERROR("Failed to transmit frame!");
        Retcode_RaiseError(rc);
    }
}

static void HandleSelfInducedTransmitTick(TimerHandle_t timer)
{
    KISO_UNUSED(timer);

    LOG_DEBUG("Performing self induced transmit...");

    /* Just in case Ethernet wasn't initialized by now... */
    if (NULL != Ethernet)
    {
        Retcode_T rc = CmdProcessor_Enqueue(AppCmdProcessor, TransmitFrame, NULL, 0U);
        if (RETCODE_OK != rc)
        {
            LOG_WARNING("Failed to enqueue Self-Induced-Transmit callback!");
            Retcode_RaiseError(rc);
        }
    }
}

static void HandleLedTimerTick(TimerHandle_t timer)
{
    KISO_UNUSED(timer);

    for (size_t i = 0; i < sizeof(Signals) / sizeof(struct SignalLed_S *); ++i)
    {
        if (Signals[i]->Scheduled)
        {
            Signals[i]->TickCounter++;
            if (Signals[i]->TickCounter >= Signals[i]->TurnOffTicks)
            {
                Signals[i]->Scheduled = false;
                Signals[i]->TickCounter = 0;

                Retcode_T rc = BSP_LED_Switch(Signals[i]->LedId, NUCLEOF767_LED_COMMAND_OFF);
                if (RETCODE_OK != rc)
                {
                    Retcode_RaiseError(rc);
                    rc = RETCODE_OK;
                }
            }
        }
    }
}

void Ethernet_Startup(void *cmdProcessor, uint32_t param2)
{
    KISO_UNUSED(param2);

    if (cmdProcessor == NULL)
    {
        LOG_FATAL("Invalid CmdProcessor passed to app startup! Aborting...\n");
        assert(false);
        return;
    }

    AppCmdProcessor = (CmdProcessor_T *)cmdProcessor;

    TransmitCompleteSignal = xSemaphoreCreateBinary();
    if (NULL == TransmitCompleteSignal)
    {
        LOG_FATAL("Could not allocate transmit-complete signal!");
        Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES));
        return;
    }

    LedTimer = xTimerCreate("LED", ETHERNET_LED_TICKPERIOD, pdTRUE, NULL, HandleLedTimerTick);
    if (NULL == LedTimer)
    {
        LOG_FATAL("Could not allocate LED timer!");
        Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES));
        return;
    }

    BaseType_t timerStarted = xTimerStart(LedTimer, 0U);
    if (!timerStarted)
    {
        LOG_FATAL("Failed to start LED timer!");
        Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES));
        return;
    }

    Retcode_T rc = BSP_Ethernet_Connect();
    if (RETCODE_OK != rc)
    {
        LOG_FATAL("Failed to connect Ethernet BSP!");
        Retcode_RaiseError(rc);
        return;
    }

    rc = BSP_Ethernet_Enable();
    if (RETCODE_OK != rc)
    {
        LOG_FATAL("Failed to enable Ethernet BSP!");
        Retcode_RaiseError(rc);
        return;
    }

    Ethernet = (Ethernet_T)BSP_Ethernet_GetEthernetHandle();
    assert(NULL != Ethernet);

    rc = MCU_Ethernet_Initialize(Ethernet, HandleEthernetEvent);
    if (RETCODE_OK != rc)
    {
        LOG_FATAL("Failed to initialize MCU Ethernet!");
        Retcode_RaiseError(rc);
        return;
    }

    for (size_t i = 0; i < ETHERNET_RXPOOL_LENGTH; ++i)
    {
        RxPool[i].Size = ETHERNET_RXPOOLBUFFER_SIZE;
        RxPool[i].Data = RxPoolBuffers[i];
    }

    struct MCU_Ethernet_PhyisicalAddress_S phyAddr;
    memcpy(phyAddr.Mac, SrcMacAddress, MCU_ETHERNET_MACLENGTH);
    rc = MCU_Ethernet_StartReceive(Ethernet, &phyAddr, RxPool, ETHERNET_RXPOOL_LENGTH);
    if (RETCODE_OK != rc)
    {
        LOG_FATAL("Failed to start MCU Ethernet rx-process!");
        Retcode_RaiseError(rc);
        return;
    }

    SelfInducedTransmitTimer = xTimerCreate("SelfInducedTransmit", ETHERNET_SELFINDUCEDTRANSMIT_TICKPERIOD, pdTRUE, NULL, HandleSelfInducedTransmitTick);
    if (NULL == SelfInducedTransmitTimer)
    {
        LOG_FATAL("Could not allocate Self-Induced-Transmit timer!");
        Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES));
        return;
    }

    timerStarted = xTimerStart(SelfInducedTransmitTimer, 0U);
    if (!timerStarted)
    {
        LOG_FATAL("Failed to start Self-Induced-Transmit timer!");
        Retcode_RaiseError(RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES));
        return;
    }

    /* On boot, indicate all LEDs once to notify app is running. */
    for (size_t i = 0; i < sizeof(Signals) / sizeof(struct SignalLed_S *); ++i)
    {
        rc = CmdProcessor_Enqueue(AppCmdProcessor, IndicateSignal, Signals[i], 0U);
        if (RETCODE_OK != rc)
        {
            LOG_FATAL("Failed to enqueue signal on boot!");
            Retcode_RaiseError(rc);
        }
    }
}
