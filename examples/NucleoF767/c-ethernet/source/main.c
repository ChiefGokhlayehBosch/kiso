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
#define KISO_MODULE_ID APP_MODULE_ID_MAIN

#include "Ethernet.h"

#include "Kiso_Retcode.h"
#include "Kiso_Assert.h"
#include "Kiso_CmdProcessor.h"
#include "Kiso_Logging.h"
#include "Kiso_BSP_Board.h"
#include "Kiso_BSP_LED.h"

#include "BSP_NucleoF767.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define MAIN_DEFAULT_CMDPROCESSOR_PRIORITY (UINT32_C(1))
#define MAIN_DEFAULT_CMDPROCESSOR_STACKSIZE (UINT16_C(700))
#define MAIN_DEFAULT_CMDPROCESSOR_QUEUELENGTH (UINT32_C(10))

void HandleError(Retcode_T error, bool isfromIsr);
void HandleAssert(const unsigned long line, const unsigned char *const file);

static Retcode_T SystemStartup(void);
static void SysTickPreCallback(void);
static void StartupLogging(void *param1, uint32_t param2);
extern void xPortSysTickHandler(void);

#if configSUPPORT_STATIC_ALLOCATION
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);
#endif
#if configSUPPORT_STATIC_ALLOCATION && configUSE_TIMERS
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize);
#endif

static CmdProcessor_T DefaultCmdProcessor;

int main(void)
{
    Retcode_T rc = Retcode_Initialize(HandleError);
    if (RETCODE_OK == rc)
    {
        rc = SystemStartup();
    }
    if (RETCODE_OK == rc)
    {
        rc = CmdProcessor_Initialize(&DefaultCmdProcessor,
                                     "DefaultCmdProcessor",
                                     MAIN_DEFAULT_CMDPROCESSOR_PRIORITY,
                                     MAIN_DEFAULT_CMDPROCESSOR_STACKSIZE,
                                     MAIN_DEFAULT_CMDPROCESSOR_QUEUELENGTH);
    }
    if (RETCODE_OK == rc)
    {
        /* Logging_Init must be done after OS-scheduler is up and running. */
        rc = CmdProcessor_Enqueue(&DefaultCmdProcessor,
                                  StartupLogging,
                                  NULL,
                                  0U);
    }
    if (RETCODE_OK == rc)
    {
        /* Here we enqueue the application initialization into the command
         * processor, such that the initialization function will be invoked
         * once the RTOS scheduler is started below. */
        rc = CmdProcessor_Enqueue(&DefaultCmdProcessor,
                                  Ethernet_Startup,
                                  &DefaultCmdProcessor,
                                  0U);
    }
    if (RETCODE_OK != rc)
    {
        printf("System Startup failed\n");
        assert(false);
    }
    /* start scheduler */
    vTaskStartScheduler();
}

/**
 * @brief Callback to execute when an error is raised by Retcode module
 */
void HandleError(Retcode_T error, bool isfromIsr)
{
    if (false == isfromIsr)
    {
        /** \todo: ERROR HANDLING SHOULD BE DONE FOR THE ERRORS RAISED FROM PLATFORM */
        uint32_t PackageID = Retcode_GetPackage(error);
        uint32_t ErrorCode = Retcode_GetCode(error);
        uint32_t ModuleID = Retcode_GetModuleId(error);
        Retcode_Severity_T SeverityCode = Retcode_GetSeverity(error);

        if (RETCODE_SEVERITY_FATAL == SeverityCode)
            printf("Fatal error from package %u , Error code: %u and module ID is :%u\n", (unsigned int)PackageID, (unsigned int)ErrorCode, (unsigned int)ModuleID);

        if (RETCODE_SEVERITY_ERROR == SeverityCode)
            printf("Severe error from package %u , Error code: %u and module ID is :%u\n", (unsigned int)PackageID, (unsigned int)ErrorCode, (unsigned int)ModuleID);
    }
    else
    {
        BSP_LED_Switch(NUCLEOF767_LED_RED_ID, NUCLEOF767_LED_COMMAND_ON);
    }
}

#ifndef NDEBUG /* valid only for debug builds */

/**
 * @brief This API is called when function enters an assert
 *
 * @param[in] line line number where asserted
 * @param[in] file file name which is asserted
 */
void HandleAssert(const unsigned long line, const unsigned char *const file)
{
    /* Switch on the LEDs */
    Retcode_T retcode = RETCODE_OK;

    retcode = BSP_LED_Switch(NUCLEOF767_LED_ALL, NUCLEOF767_LED_COMMAND_ON);

    printf("Asserted in File %s, line no.: %ld\n", file, line);

    if (retcode != RETCODE_OK)
    {
        printf("LED's ON failed during assert\n");
    }
}
#endif

/**
 * @brief Initializes the board and required peripherals
 */
static Retcode_T SystemStartup(void)
{
    Retcode_T rc = RETCODE_OK;
    uint32_t param1 = 0;
    void *param2 = NULL;

    /* Initialize the callbacks for the system tick */
    BSP_Board_OSTickInitialize(SysTickPreCallback, NULL);

#ifndef NDEBUG /* valid only for debug builds */
    if (RETCODE_OK == rc)
    {
        rc = Assert_Initialize(HandleAssert);
    }
#endif
    /* First we initialize the board. */
    if (RETCODE_OK == rc)
    {
        rc = BSP_Board_Initialize(param1, param2);
    }
    if (RETCODE_OK == rc)
    {
        rc = BSP_LED_Connect();
    }

    if (RETCODE_OK == rc)
    {
        rc = BSP_LED_Enable(NUCLEOF767_LED_ALL);
    }

    return rc;
}

static void StartupLogging(void *param1, uint32_t param2)
{
    KISO_UNUSED(param1);
    KISO_UNUSED(param2);

    Retcode_T optRc = Logging_Init(Logging_SyncRecorder, Logging_UARTAppender);
    if (RETCODE_OK == optRc)
    {
        LOG_DEBUG("Logging started.");
    }
}

/**
 * @brief Pre-SysTick callback to be called before the primary SysTick handler.
 *
 * This is called whenever the SysTick-IRQ is given. This is a temporary
 * implementation where the `SysTick_Handler()` is not directly mapped to
 * `xPortSysTickHandler()`. Instead it is only called if the scheduler has
 * started.
 */
static void SysTickPreCallback(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

#if configSUPPORT_STATIC_ALLOCATION

static StaticTask_t IdleTaskTcbBuffer;
static StackType_t IdleStack[IDLE_TASK_SIZE];

/**
 * @brief Provides the caller with a pre-allocated TCB and stack buffer for the
 * FreeRTOS idle task.
 *
 * If static allocation are supported then the application must
 * provide this callback function.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &IdleTaskTcbBuffer;
    *ppxIdleTaskStackBuffer = IdleStack;
    *pulIdleTaskStackSize = IDLE_TASK_SIZE;
}

#if configUSE_TIMERS

static StaticTask_t TimerTaskTCBBuffer;
static StackType_t TimerStack[configTIMER_TASK_STACK_DEPTH];

/**
 * @brief Provides the caller with a pre-allocated TCB and stack buffer for the
 * FreeRTOS sw-timer task.
 *
 * If static allocation and timers are supported then the application must
 * provide this callback function.
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &TimerTaskTCBBuffer;
    *ppxTimerTaskStackBuffer = TimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif /* configUSE_TIMERS */

#endif /* configSUPPORT_STATIC_ALLOCATION */
