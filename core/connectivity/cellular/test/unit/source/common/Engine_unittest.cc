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
 *    Robert Bosch GmbH - transceiver based AT command parsing
 *    Robert Bosch GmbH - initial contribution
 *
 ******************************************************************************/

#include <fff.h>
#include <gtest.h>

extern "C"
{
#include "Kiso_CellularModules.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ENGINE

#include "FreeRTOS_th.hh"
#include "task_th.hh"
#include "semphr_th.hh"
#include "portmacro_th.hh"

#include "Urc_th.hh"
#include "Hardware_th.hh"
#include "AtTransceiver_th.hh"

#include "Kiso_Retcode_th.hh"
#include "Kiso_Assert_th.hh"

#include "Kiso_Logging_th.hh"
#include "Kiso_MCU_UART_th.hh"

#undef KISO_MODULE_ID
#include "Engine.c"
}

class TS_InitializedEngine : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(AtTransceiver_Lock);
        RESET_FAKE(AtTransceiver_Unlock);
        RESET_FAKE(AtTransceiver_PrepareWrite);
        RESET_FAKE(AtTransceiver_Feed);
        RESET_FAKE(AtTransceiver_Deinitialize);

        RESET_FAKE(Hardware_Deinitialize);

        RESET_FAKE(Urc_HandleResponses);

        RESET_FAKE(xSemaphoreTake);
        RESET_FAKE(xSemaphoreGiveFromISR);
        RESET_FAKE(vTaskDelete);
        RESET_FAKE(vQueueDelete);

        RESET_FAKE(Retcode_RaiseError);
        RESET_FAKE(Retcode_RaiseErrorFromIsr);

        RESET_FAKE(MCU_UART_Send);

        IsEchoModeEnabled = true;
        RxWakeupSignal = (SemaphoreHandle_t)123;
        TxWakeupSignal = (SemaphoreHandle_t)321;
        IdleUrcListenerTask = (TaskHandle_t)123;
        Serial = (UART_T)987;
    }
};

class TS_UninitializedEngine : public TS_InitializedEngine
{
protected:
    virtual void SetUp() override
    {
        TS_InitializedEngine::SetUp();

        RESET_FAKE(xSemaphoreCreateBinaryStatic);
        RESET_FAKE(Hardware_Initialize);
        RESET_FAKE(Hardware_GetCommunicationChannel);
        RESET_FAKE(AtTransceiver_Initialize);
        RESET_FAKE(xTaskCreateStatic);

        IsEchoModeEnabled = false;
        RxWakeupSignal = NULL;
        TxWakeupSignal = NULL;
        IdleUrcListenerTask = NULL;
        Serial = (UART_T)0;
    }
};

class TS_Engine_Initialize : public TS_UninitializedEngine
{
};

TEST_F(TS_Engine_Initialize, Given__uninitialized_module__When__initializing__Then__create_signals__create_task__init_hardware__init_transceiver__return_ok)
{
    xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)321;
    xTaskCreateStatic_fake.return_val = (TaskHandle_t)123;

    Retcode_T rc = Engine_Initialize();

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(2U, xSemaphoreCreateBinaryStatic_fake.call_count);
    ASSERT_EQ(1U, xTaskCreateStatic_fake.call_count);
    ASSERT_EQ(1U, Hardware_Initialize_fake.call_count);
    ASSERT_EQ(1U, Hardware_GetCommunicationChannel_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_Initialize_fake.call_count);
    ASSERT_TRUE(IsEchoModeEnabled);
}

TEST_F(TS_Engine_Initialize, Given__uninitialized_module__failure_during_hw_init__When__initializing__Then__create_signals__create_task__abort_init__return_hw_fault)
{
    xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)321;
    Hardware_Initialize_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Retcode_T rc = Engine_Initialize();

    ASSERT_EQ(Hardware_Initialize_fake.return_val, rc);
    ASSERT_EQ(2U, xSemaphoreCreateBinaryStatic_fake.call_count);
    ASSERT_EQ(1U, xTaskCreateStatic_fake.call_count);
    ASSERT_EQ(1U, Hardware_Initialize_fake.call_count);
    ASSERT_EQ(0U, Hardware_GetCommunicationChannel_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_Initialize_fake.call_count);
    ASSERT_FALSE(IsEchoModeEnabled);
}

TEST_F(TS_Engine_Initialize, Given__uninitialized_module__failure_obtaining_comm_channel__When__initializing__Then__create_signals__create_task__init_hardware__abort_init__return_hw_fault)
{
    xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)321;
    Hardware_GetCommunicationChannel_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Retcode_T rc = Engine_Initialize();

    ASSERT_EQ(Hardware_GetCommunicationChannel_fake.return_val, rc);
    ASSERT_EQ(2U, xSemaphoreCreateBinaryStatic_fake.call_count);
    ASSERT_EQ(1U, xTaskCreateStatic_fake.call_count);
    ASSERT_EQ(1U, Hardware_Initialize_fake.call_count);
    ASSERT_EQ(1U, Hardware_GetCommunicationChannel_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_Initialize_fake.call_count);
    ASSERT_FALSE(IsEchoModeEnabled);
}

TEST_F(TS_Engine_Initialize, Given__uninitialized_module__transceiver_init_failure__When__initializing__Then__create_signals__create_task__init_hardware__abort_init__return_transceiver_fault)
{
    xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)321;
    AtTransceiver_Initialize_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Retcode_T rc = Engine_Initialize();

    ASSERT_EQ(AtTransceiver_Initialize_fake.return_val, rc);
    ASSERT_EQ(2U, xSemaphoreCreateBinaryStatic_fake.call_count);
    ASSERT_EQ(1U, xTaskCreateStatic_fake.call_count);
    ASSERT_EQ(1U, Hardware_Initialize_fake.call_count);
    ASSERT_EQ(1U, Hardware_GetCommunicationChannel_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_Initialize_fake.call_count);
    ASSERT_FALSE(IsEchoModeEnabled);
}

class TS_Engine_SetEchoMode : public TS_InitializedEngine
{
};

TEST_F(TS_Engine_SetEchoMode, Given__echo_mode_disabled__When__enabling_echo_mode__Then__echo_mode_enabled)
{
    IsEchoModeEnabled = false;

    Engine_SetEchoMode(true);

    ASSERT_TRUE(IsEchoModeEnabled);
}

TEST_F(TS_Engine_SetEchoMode, Given__echo_mode_enabled__When__disabling_echo_mode__Then__echo_mode_disabled)
{
    IsEchoModeEnabled = true;

    Engine_SetEchoMode(false);

    ASSERT_FALSE(IsEchoModeEnabled);
}

class TS_Engine_GetEchoMode : public TS_InitializedEngine
{
};

TEST_F(TS_Engine_GetEchoMode, Given__echo_mode_enabled__When__getting_echo_mode__Then__return_true)
{
    IsEchoModeEnabled = true;

    bool echoMode = Engine_GetEchoMode();

    ASSERT_TRUE(echoMode);
}

TEST_F(TS_Engine_GetEchoMode, Given__echo_mode_disabled__When__getting_echo_mode__Then__return_false)
{
    IsEchoModeEnabled = false;

    bool echoMode = Engine_GetEchoMode();

    ASSERT_FALSE(echoMode);
}

class TS_Engine_OpenTransceiver : public TS_InitializedEngine
{
};

TEST_F(TS_Engine_OpenTransceiver, Given__valid_out_ptr__echo_mode_enabled__When__opening_transceiver__Then__prepare_transceiver_in_echo_mode__return_ok)
{
    IsEchoModeEnabled = true;
    struct AtTransceiver_S *transceiver = NULL;

    Retcode_T rc = Engine_OpenTransceiver(&transceiver);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_NE(nullptr, transceiver);
    ASSERT_EQ(1U, AtTransceiver_Lock_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_PrepareWrite_fake.call_count);
    ASSERT_TRUE(AtTransceiver_PrepareWrite_fake.arg1_val & (ATTRANSCEIVER_WRITEOPTION_DEFAULT | ATTRANSCEIVER_WRITEOPTION_NOBUFFER));
    ASSERT_EQ(nullptr, AtTransceiver_PrepareWrite_fake.arg2_val);
    ASSERT_EQ(0U, AtTransceiver_PrepareWrite_fake.arg3_val);
    ASSERT_FALSE(AtTransceiver_PrepareWrite_fake.arg1_val & ATTRANSCEIVER_WRITEOPTION_NOECHO);
    ASSERT_EQ(0U, AtTransceiver_Unlock_fake.call_count);
}

TEST_F(TS_Engine_OpenTransceiver, Given__valid_out_ptr__echo_mode_disabled__When__opening_transceiver__Then__prepare_transceiver_in_no_echo_mode__return_ok)
{
    IsEchoModeEnabled = false;
    struct AtTransceiver_S *transceiver = NULL;

    Retcode_T rc = Engine_OpenTransceiver(&transceiver);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_NE(nullptr, transceiver);
    ASSERT_EQ(1U, AtTransceiver_Lock_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_PrepareWrite_fake.call_count);
    ASSERT_TRUE(AtTransceiver_PrepareWrite_fake.arg1_val & (ATTRANSCEIVER_WRITEOPTION_DEFAULT | ATTRANSCEIVER_WRITEOPTION_NOBUFFER));
    ASSERT_EQ(nullptr, AtTransceiver_PrepareWrite_fake.arg2_val);
    ASSERT_EQ(0U, AtTransceiver_PrepareWrite_fake.arg3_val);
    ASSERT_TRUE(AtTransceiver_PrepareWrite_fake.arg1_val & ATTRANSCEIVER_WRITEOPTION_NOECHO);
    ASSERT_EQ(0U, AtTransceiver_Unlock_fake.call_count);
}

TEST_F(TS_Engine_OpenTransceiver, Given__invalid_out_ptr__When__opening_transceiver__Then__return_null_pointer_error)
{
    Retcode_T rc = Engine_OpenTransceiver(NULL);

    ASSERT_EQ(RETCODE_NULL_POINTER, Retcode_GetCode(rc));
    ASSERT_EQ(0U, AtTransceiver_Lock_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_PrepareWrite_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_Unlock_fake.call_count);
}

TEST_F(TS_Engine_OpenTransceiver, Given__valid_out_ptr__echo_mode_disabled__transceiver_lock_failure__When__opening_transceiver__Then__return_transceiver_lock_failure)
{
    IsEchoModeEnabled = false;
    struct AtTransceiver_S *transceiver = NULL;

    AtTransceiver_Lock_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Retcode_T rc = Engine_OpenTransceiver(&transceiver);

    ASSERT_EQ(AtTransceiver_Lock_fake.return_val, rc);
    ASSERT_EQ(nullptr, transceiver);
    ASSERT_EQ(1U, AtTransceiver_Lock_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_PrepareWrite_fake.call_count);
    ASSERT_EQ(0U, AtTransceiver_Unlock_fake.call_count);
}

TEST_F(TS_Engine_OpenTransceiver, Given__valid_out_ptr__echo_mode_disabled__transceiver_prepare_write_failure__When__opening_transceiver__Then__unlock_transceiver__return_transceiver_prepare_write_failure)
{
    IsEchoModeEnabled = false;
    struct AtTransceiver_S *transceiver = NULL;

    AtTransceiver_PrepareWrite_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Retcode_T rc = Engine_OpenTransceiver(&transceiver);

    ASSERT_EQ(AtTransceiver_PrepareWrite_fake.return_val, rc);
    ASSERT_EQ(nullptr, transceiver);
    ASSERT_EQ(1U, AtTransceiver_Lock_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_PrepareWrite_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_Unlock_fake.call_count);
}

class TS_Engine_CloseTransceiver : public TS_InitializedEngine
{
};

TEST_F(TS_Engine_CloseTransceiver, Given__locked_transceiver__When__closing_transceiver__Then__unlock_transceiver__return_ok)
{
    AtTransceiver_Unlock_fake.return_val = RETCODE_OK;

    Retcode_T rc = Engine_CloseTransceiver();

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(1U, AtTransceiver_Unlock_fake.call_count);
}

class TS_Engine_Deinitialize : public TS_InitializedEngine
{
};

TEST_F(TS_Engine_Deinitialize, Given____When__deinitializeing_engine__Then__no_errors_raised)
{
    Engine_Deinitialize();

    ASSERT_EQ(0U, Retcode_RaiseError_fake.call_count);
    ASSERT_EQ(2U, vQueueDelete_fake.call_count);
    ASSERT_EQ(1U, vTaskDelete_fake.call_count);
    ASSERT_EQ(1U, Hardware_Deinitialize_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_Deinitialize_fake.call_count);
    ASSERT_EQ(NULL, RxWakeupSignal);
    ASSERT_EQ(NULL, TxWakeupSignal);
    ASSERT_EQ(NULL, IdleUrcListenerTask);
    ASSERT_EQ((UART_T)0, Serial);
}

TEST_F(TS_Engine_Deinitialize, Given__hw_deinit_failure__When__deinitializeing_engine__Then__raise_hw_init_failure__continue_deinit)
{
    Hardware_Deinitialize_fake.return_val = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);

    Engine_Deinitialize();

    ASSERT_EQ(1U, Retcode_RaiseError_fake.call_count);
    ASSERT_EQ(Hardware_Deinitialize_fake.return_val, Retcode_RaiseError_fake.arg0_val);
    ASSERT_EQ(2U, vQueueDelete_fake.call_count);
    ASSERT_EQ(1U, vTaskDelete_fake.call_count);
    ASSERT_EQ(1U, Hardware_Deinitialize_fake.call_count);
    ASSERT_EQ(1U, AtTransceiver_Deinitialize_fake.call_count);
    ASSERT_EQ(NULL, RxWakeupSignal);
    ASSERT_EQ(NULL, TxWakeupSignal);
    ASSERT_EQ(NULL, IdleUrcListenerTask);
    ASSERT_EQ((UART_T)0, Serial);
}
