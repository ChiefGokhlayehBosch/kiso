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

/** LCOV_EXCL_START : start of section where code coverage is ignored. */

#include <gtest.h>
#include <fff.h>
#include <queue>
#include <random>

extern "C"
{
#include "Kiso_Assert_th.hh"
#include "Kiso_Retcode_th.hh"
#include "Kiso_RingBuffer_th.hh"
#include "FreeRTOS_th.hh"
#include "task_th.hh"
#include "semphr_th.hh"

#undef KISO_MODULE_ID
#include "AtTransceiver.c"
}

static std::stringstream WrittenTransceiverData;
FAKE_VALUE_FUNC(Retcode_T, FakeTransceiverWriteFunction, const void *, size_t, size_t *);

static Retcode_T Fake_AtTransceiver_WriteFunction(const void *data, size_t length, size_t *numBytesWritten)
{
    for (size_t i = 0; i < length; ++i)
    {
        WrittenTransceiverData.put(((const char *)data)[i]);
    }

    if (numBytesWritten)
        *numBytesWritten = length;
    return RETCODE_OK;
}

static void Fake_RingBuffer_Initialize(RingBuffer_T *rb, uint8_t *buf, uint32_t len)
{
    KISO_UNUSED(len);
    rb->Base = buf;
    rb->Rptr = nullptr;
    rb->Wptr = nullptr;
    rb->Size = 0;
}

static uint32_t Fake_RingBuffer_Read(RingBuffer_T *rb, uint8_t *buf, uint32_t len)
{
    std::queue<uint8_t> *q = reinterpret_cast<std::queue<uint8_t> *>(rb->Base);
    size_t read;

    for (read = 0; read < len && !q->empty(); ++read, q->pop())
    {
        buf[read] = q->front();
    }

    return read;
}

static uint32_t Fake_RingBuffer_Peek(RingBuffer_T *rb, uint8_t *buf, uint32_t len)
{
    std::queue<uint8_t> *q = reinterpret_cast<std::queue<uint8_t> *>(rb->Base);
    std::queue<uint8_t> qCopy(*q);
    size_t read;

    for (read = 0; read < len && !qCopy.empty(); ++read, qCopy.pop())
    {
        buf[read] = qCopy.front();
    }

    return read;
}

static uint32_t Fake_RingBuffer_Write(RingBuffer_T *rb, const uint8_t *buf, uint32_t len)
{
    std::queue<uint8_t> *q = reinterpret_cast<std::queue<uint8_t> *>(rb->Base);
    size_t written;

    for (written = 0; written < len; ++written)
    {
        q->push(buf[written]);
    }

    return written;
}

/**
 * @brief Little helper for generating random numbers in specified range.
 */
int random(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 engine(rd());

    std::uniform_int_distribution<> dist(min, max);

    return dist(engine);
}

class TS_ReadableAndWritableTransceiver : public testing::Test
{
protected:
    struct AtTransceiver_S transceiver;
    struct AtTransceiver_S *t = &transceiver;
    std::queue<uint8_t> transceiverRxBuffer;

    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(xSemaphoreCreateBinaryStatic);
        RESET_FAKE(xSemaphoreCreateMutexStatic);
        RESET_FAKE(RingBuffer_Initialize);
        RESET_FAKE(RingBuffer_Read);
        RESET_FAKE(RingBuffer_Peek);
        RESET_FAKE(RingBuffer_Write);
        RESET_FAKE(FakeTransceiverWriteFunction);

        WrittenTransceiverData.str("");
        WrittenTransceiverData.clear();

        xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)1;
        xSemaphoreCreateMutexStatic_fake.return_val = (SemaphoreHandle_t)1;
        RingBuffer_Initialize_fake.custom_fake = Fake_RingBuffer_Initialize;
        RingBuffer_Read_fake.custom_fake = Fake_RingBuffer_Read;
        RingBuffer_Peek_fake.custom_fake = Fake_RingBuffer_Peek;
        RingBuffer_Write_fake.custom_fake = Fake_RingBuffer_Write;
        FakeTransceiverWriteFunction_fake.custom_fake = Fake_AtTransceiver_WriteFunction;

        Retcode_T rc = AtTransceiver_Initialize(t, reinterpret_cast<uint8_t *>(&transceiverRxBuffer), 0, FakeTransceiverWriteFunction);
        if (RETCODE_OK != rc)
        {
            std::cerr << "Failed to set up TS_ReadableAndWritableTransceiver" << std::endl;
            exit(1);
        }

        rc = AtTransceiver_PrepareWrite(t, (enum AtTransceiver_WriteOption_E)((int)ATTRANSCEIVER_WRITEOPTION_NOBUFFER | (int)ATTRANSCEIVER_WRITEOPTION_NOECHO), nullptr, 0);
        if (RETCODE_OK != rc)
        {
            std::cerr << "Failed to set up TS_ReadableAndWritableTransceiver" << std::endl;
            exit(1);
        }
    }
};

/** LCOV_EXCL_STOP : end of section where code coverage is ignored. */
