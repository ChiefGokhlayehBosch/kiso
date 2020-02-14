/*
 * Copyright (c) 2010-2019 Robert Bosch GmbH
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *     Robert Bosch GmbH - initial contribution
 */

#include <gtest.h>
#include <queue>
#include <climits>
#include <sstream>
#include <ios>

FFF_DEFINITION_BLOCK_START

extern "C"
{
#include "Kiso_Cellular.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ATTRANSCEIVER

#include "Hardware_th.hh"

#include "Kiso_Retcode_th.hh"
#include "Kiso_Assert_th.hh"

#include "Kiso_RingBuffer_th.hh"
#include "Kiso_Logging_th.hh"

#include "FreeRTOS_th.hh"
#include "portmacro_th.hh"
#include "semphr_th.hh"
#include "task_th.hh"

#include "AtTransceiver.c"
}

FFF_DEFINITION_BLOCK_END

using namespace std;

/**
 * @brief Little helper for generating random numbers in specified range.
 */
int random(int min, int max)
{
    static bool first = true;
    if (first)
    {
        srand(time(nullptr)); //seeding for the first time only!
        first = false;
    }
    return min + rand() % ((max + 1) - min);
}

static uint32_t FakeRx_RingBuffer_Read(RingBuffer_T *rb, uint8_t *buf, uint32_t len);
static uint32_t FakeRx_RingBuffer_Peek(RingBuffer_T *rb, uint8_t *buf, uint32_t len);
static BaseType_t FakeRx_xSemaphoreTake(SemaphoreHandle_t semphr, TickType_t timeout);

class Message : public queue<uint8_t>
{
private:
    const TickType_t delay;

public:
    Message(const void *buf, size_t len, TickType_t _delay = 0)
        : delay(_delay)
    {
        for (size_t i = 0; i < len; ++i)
            push(((const uint8_t *)buf)[i]);
    }
    Message(const string &str, TickType_t _delay = 0)
        : delay(_delay)
    {
        for (size_t i = 0; i < str.length(); ++i)
            push(str[i]);
    }
    TickType_t GetDelay() const
    {
        return delay;
    }
};

class TS_FakeRx : public testing::Test
{
private:
    queue<Message> messages;
    queue<uint8_t> fakeRxBytes;
    size_t totalDequeuedCount;

protected:
    struct AtTransceiver_S t;

protected:
    virtual void SetUp()
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(RingBuffer_Read);
        RESET_FAKE(RingBuffer_Peek);
        RESET_FAKE(RingBuffer_Write);
        RESET_FAKE(xTaskGetTickCount);

        RingBuffer_Read_fake.custom_fake = FakeRx_RingBuffer_Read;
        RingBuffer_Peek_fake.custom_fake = FakeRx_RingBuffer_Peek;
        xSemaphoreTake_fake.custom_fake = FakeRx_xSemaphoreTake;

        t.RxRingBuffer.Base = (uint8_t *)&fakeRxBytes;
        t.RxWakeupHandle = (SemaphoreHandle_t)this;

        totalDequeuedCount = 0;
    }
    virtual void TearDown()
    {
        while (!messages.empty())
        {
            messages.pop();
        }
    }
    void EnqueueMessage(const string &str, TickType_t delay = 0)
    {
        bool hasNoMessages = messages.empty();
        messages.push(Message(str, delay));

        /* Make sure, that messages with no delay are readable immediately. */
        if (hasNoMessages && delay == 0)
        {
            DequeueMessage();
        }
    }
    void EnqueueMessage(const void *data, size_t length, TickType_t delay = 0)
    {
        bool hasNoMessages = messages.empty();
        messages.push(Message(data, length, delay));

        /* Make sure, that messages with no delay are readable immediately. */
        if (hasNoMessages && delay == 0)
        {
            DequeueMessage();
        }
    }
    bool DequeueMessage()
    {
        if (!messages.empty())
        {
            Message &m = messages.front();
            while (!m.empty())
            {
                fakeRxBytes.push(m.front());
                m.pop();
                totalDequeuedCount++;
            }
            messages.pop();

            xTaskGetTickCount_fake.return_val += m.GetDelay();
            return true;
        }
        else
        {
            return false;
        }
    }
    size_t GetFakeRxByteCount() const
    {
        return fakeRxBytes.size();
    }
    size_t GetTotalDequeuedByteCount() const
    {
        return totalDequeuedCount;
    }

    friend BaseType_t FakeRx_xSemaphoreTake(SemaphoreHandle_t semphr, TickType_t timeout);
};

static uint32_t FakeRx_RingBuffer_Read(RingBuffer_T *rb, uint8_t *buf, uint32_t len)
{
    queue<uint8_t> *q = reinterpret_cast<queue<uint8_t> *>(rb->Base);
    size_t read;

    for (read = 0; read < len && !q->empty(); ++read, q->pop())
    {
        buf[read] = q->front();
    }

    return read;
}

static uint32_t FakeRx_RingBuffer_Peek(RingBuffer_T *rb, uint8_t *buf, uint32_t len)
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

static BaseType_t FakeRx_xSemaphoreTake(SemaphoreHandle_t semphr, TickType_t timeout)
{
    KISO_UNUSED(timeout);
    TS_FakeRx *f = reinterpret_cast<TS_FakeRx *>(semphr);

    if (f->DequeueMessage())
    {
        return pdPASS;
    }
    else
    {
        return pdFAIL;
    }
}

TEST_F(TS_FakeRx, Smoke_EnqueueAndTake_Success)
{
    const char *expData = "AT+COPS=0\r\n";
    size_t expDataLen = strlen(expData);
    uint8_t actData[expDataLen * 2];
    memset(actData, 0, sizeof(actData));

    this->EnqueueMessage(expData, expDataLen);
    xSemaphoreTake(t.RxWakeupHandle, portMAX_DELAY);
    uint32_t read = RingBuffer_Read(&t.RxRingBuffer, actData, sizeof(actData));

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, (char *)actData);
}

TEST_F(TS_FakeRx, Smoke_EnqueueAndTakeMultiple_Success)
{
    const char *str = "AT+COPS=0\r\n";
    char expData[strlen(str) + 1];
    strcpy(expData, str);
    size_t expDataLen = strlen(expData);
    uint8_t actData[expDataLen * 2];
    memset(actData, 0, sizeof(actData));

    for (int i = 0; i < 10; ++i)
    {
        expData[0] += i;
        this->EnqueueMessage(expData, expDataLen);
        xSemaphoreTake(t.RxWakeupHandle, portMAX_DELAY);
        uint32_t read = RingBuffer_Read(&t.RxRingBuffer, actData, sizeof(actData));

        ASSERT_EQ(expDataLen, read);
        actData[read] = '\0';
        ASSERT_STREQ(expData, (char *)actData);
    }
}

class TS_Peek : public TS_FakeRx
{
};

TEST_F(TS_Peek, OneMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, expDataLen);

    size_t read = Peek(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_Peek, TwoMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *expData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData1, expDataLen1);
    this->EnqueueMessage(expData2, expDataLen2);

    size_t read = Peek(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_Peek, ManyMsg_Success)
{
    size_t expDataLen = random(500, 2000);
    cout << "Total expected data length " << expDataLen << " bytes." << endl;
    char expData[expDataLen + 1];
    for (size_t i = 0; i < expDataLen; ++i)
    {
        expData[i] = random('A', 'Z');
    }
    expData[expDataLen] = '\0';
    size_t enqueuedByteCount = 0;
    while (enqueuedByteCount < expDataLen)
    {
        size_t chunkSize = random(1, expDataLen - enqueuedByteCount);
        cout << "Enqueueing chunk of " << chunkSize << " bytes..." << endl;
        this->EnqueueMessage(expData + enqueuedByteCount, chunkSize);
        enqueuedByteCount += chunkSize;
    }
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));

    size_t read = Peek(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_Peek, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, expDataLen, tickLimit * 2);

    size_t read = Peek(&t, actData, expDataLen, &tickLimit);

    ASSERT_EQ(0U, read);
    actData[read] = '\0';
    ASSERT_STRNE(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_Peek, TwoMsgWithTimeoutInBetween_PartialSuccess)
{
    TickType_t tickLimit = 10;
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *expData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData1, expDataLen1, tickLimit);
    this->EnqueueMessage(expData2, expDataLen2, 100);

    size_t read = Peek(&t, actData, expDataLen, &tickLimit);

    ASSERT_EQ(expDataLen1, read);
    actData[read] = '\0';
    ASSERT_EQ(0, strncmp(expData1, actData, read));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_Peek, ManyMsgWithTimeoutInBetween_PartialSuccess)
{
    TickType_t tickLimit = random(1000, 10000);
    TickType_t totalEnqueuedDelay = 0;
    size_t toBeEnqueuedCount = random(500, 2000);
    size_t expPartialDataLen = 0;
    cout << "Total to-be-enqueued data length " << toBeEnqueuedCount << " bytes." << endl;
    cout << "Will limit timeout to " << tickLimit << " ticks." << endl;
    char expData[toBeEnqueuedCount + 1];
    for (size_t i = 0; i < toBeEnqueuedCount; ++i)
    {
        expData[i] = random('A', 'Z');
    }
    expData[toBeEnqueuedCount] = '\0';
    size_t enqueuedByteCount = 0;
    while (enqueuedByteCount < toBeEnqueuedCount)
    {
        size_t chunkSize = random(1, toBeEnqueuedCount - enqueuedByteCount);
        TickType_t chunkDelay = 0;
        if (enqueuedByteCount + chunkSize >= toBeEnqueuedCount)
        {
            chunkDelay = tickLimit; /* Make sure the last message will always time out */
        }
        else
        {
            chunkDelay = random(1, tickLimit);
        }
        totalEnqueuedDelay += chunkDelay;
        if (totalEnqueuedDelay <= tickLimit)
        {
            expPartialDataLen += chunkSize;
        }
        else
        {
            static bool printed = false;
            if (!printed)
            {
                cout << ">>>Timout should happen here<<<" << endl;
                printed = true;
            }
        }
        cout << "Enqueueing chunk of " << chunkSize << " bytes with delay of " << chunkDelay << " ticks..." << endl;

        this->EnqueueMessage(expData + enqueuedByteCount, chunkSize, chunkDelay);
        enqueuedByteCount += chunkSize;
    }
    char actData[enqueuedByteCount + 1];
    memset(actData, 0U, sizeof(actData));

    size_t read = Peek(&t, actData, enqueuedByteCount, &tickLimit);

    ASSERT_EQ(expPartialDataLen, read);
    actData[read] = '\0';
    ASSERT_EQ(0, strncmp(expData, actData, read));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

class TS_Pop : public TS_FakeRx
{
};

TEST_F(TS_Pop, OneMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, expDataLen);

    size_t read = Pop(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

TEST_F(TS_Pop, TwoMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *expData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData1, expDataLen1);
    this->EnqueueMessage(expData2, expDataLen2);

    size_t read = Pop(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

TEST_F(TS_Pop, ManyMsg_Success)
{
    size_t expDataLen = random(500, 2000);
    cout << "Total expected data length " << expDataLen << " bytes." << endl;
    char expData[expDataLen + 1];
    for (size_t i = 0; i < expDataLen; ++i)
    {
        expData[i] = random('A', 'Z');
    }
    expData[expDataLen] = '\0';
    size_t enqueuedByteCount = 0;
    while (enqueuedByteCount < expDataLen)
    {
        size_t chunkSize = random(1, expDataLen - enqueuedByteCount);
        cout << "Enqueueing chunk of " << chunkSize << " bytes..." << endl;
        this->EnqueueMessage(expData + enqueuedByteCount, chunkSize);
        enqueuedByteCount += chunkSize;
    }
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));

    size_t read = Pop(&t, actData, expDataLen, NULL);

    ASSERT_EQ(expDataLen, read);
    actData[read] = '\0';
    ASSERT_STREQ(expData, actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

TEST_F(TS_Pop, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    string expData = "+COPS:0,2,3,4\r\n";
    char actData[expData.length() + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, tickLimit * 2);

    size_t read = Pop(&t, actData, expData.length(), &tickLimit);

    ASSERT_EQ(0U, read);
    actData[read] = '\0';
    ASSERT_STRNE(expData.c_str(), actData);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

TEST_F(TS_Pop, TwoMsgWithTimeoutInBetween_PartialSuccess)
{
    TickType_t tickLimit = 10;
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *expData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    char actData[expDataLen + 1];
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData1, expDataLen1, tickLimit);
    this->EnqueueMessage(expData2, expDataLen2, 100);

    size_t read = Pop(&t, actData, expDataLen, &tickLimit);

    ASSERT_EQ(expDataLen1, read);
    actData[read] = '\0';
    ASSERT_EQ(0, strncmp(expData1, actData, read));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

TEST_F(TS_Pop, ManyMsgWithTimeoutInBetween_PartialSuccess)
{
    TickType_t tickLimit = random(1000, 10000);
    TickType_t totalEnqueuedDelay = 0;
    size_t toBeEnqueuedCount = random(500, 2000);
    size_t expPartialDataLen = 0;
    cout << "Total to-be-enqueued data length " << toBeEnqueuedCount << " bytes." << endl;
    cout << "Will limit timeout to " << tickLimit << " ticks." << endl;
    char expData[toBeEnqueuedCount + 1];
    for (size_t i = 0; i < toBeEnqueuedCount; ++i)
    {
        expData[i] = random('A', 'Z');
    }
    expData[toBeEnqueuedCount] = '\0';
    size_t enqueuedByteCount = 0;
    while (enqueuedByteCount < toBeEnqueuedCount)
    {
        size_t chunkSize = random(1, toBeEnqueuedCount - enqueuedByteCount);
        TickType_t chunkDelay = 0;
        if (enqueuedByteCount + chunkSize >= toBeEnqueuedCount)
        {
            chunkDelay = tickLimit; /* Make sure the last message will always time out */
        }
        else
        {
            chunkDelay = random(1, tickLimit);
        }
        totalEnqueuedDelay += chunkDelay;
        if (totalEnqueuedDelay <= tickLimit)
        {
            expPartialDataLen += chunkSize;
        }
        else
        {
            static bool printed = false;
            if (!printed)
            {
                cout << ">>>Timout should happen here<<<" << endl;
                printed = true;
            }
        }
        cout << "Enqueueing chunk of " << chunkSize << " bytes with delay of " << chunkDelay << " ticks..." << endl;

        this->EnqueueMessage(expData + enqueuedByteCount, chunkSize, chunkDelay);
        enqueuedByteCount += chunkSize;
    }
    char actData[enqueuedByteCount + 1];
    memset(actData, 0U, sizeof(actData));

    size_t read = Pop(&t, actData, enqueuedByteCount, &tickLimit);

    ASSERT_EQ(expPartialDataLen, read);
    actData[read] = '\0';
    ASSERT_EQ(0, strncmp(expData, actData, read));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - read, this->GetFakeRxByteCount());
}

class TS_PopUntil : public TS_FakeRx
{
};

TEST_F(TS_PopUntil, CrNearEndOfString_Success)
{
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, LfEndOfString_Success)
{
    const char *needle = "\n";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, CrOrLfNearEndOfString_Success)
{
    const char *needle = "\r\n";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, CrInSecondMessage_Success)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string totalData1 = totalData.substr(0, totalData.length() / 2);
    string totalData2 = totalData.substr(totalData1.length());
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData1, tickLimit / 2);
    this->EnqueueMessage(totalData2, tickLimit / 2);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, LfBeginningOfString_Success)
{
    const char *needle = "\n";
    string totalData = "\n+COPS:0,2,3,4";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, LfBeginningOfStringIgnoreMultipleOccurences_Success)
{
    const char *needle = "\n";
    string totalData = "\n+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, LfOnlyCharInString_Success)
{
    const char *needle = "\n";
    string totalData = "\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, InsufficentSearchBuffer_Fail)
{
    const char *needle = "\n";
    string expData = "+COPS:0,2,3,4";
    char actData[expData.length() + 1];
    size_t actDataLen = expData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES), rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 1000;
    const char *needle = "\n";
    string expData = "+COPS:0,2,3,4";
    char actData[expData.length() + 1];
    size_t actDataLen = expData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, tickLimit * 2);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(0U, actDataLen);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen, this->GetFakeRxByteCount());
}

TEST_F(TS_PopUntil, TwoMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string totalData1 = totalData.substr(0, totalData.length() / 2);
    string totalData2 = totalData.substr(totalData.length() / 2);
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData1, tickLimit);
    this->EnqueueMessage(totalData2, 100);

    Retcode_T rc = PopUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(totalData1.length(), actDataLen);
    ASSERT_EQ(this->GetTotalDequeuedByteCount() - actDataLen, this->GetFakeRxByteCount());
}

class TS_PeekUntil : public TS_FakeRx
{
};

TEST_F(TS_PeekUntil, CrNearEndOfString_Success)
{
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, LfEndOfString_Success)
{
    const char *needle = "\n";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, LfStartOfString_Success)
{
    const char *needle = "\n";
    string totalData = "\n+COPS:0,2,3,4";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, LfStartOfStringIgnoreMultipleOccurrences_Success)
{
    const char *needle = "\n";
    string totalData = "\n+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, CrOrLfNearEndOfString_Success)
{
    const char *needle = "\r\n";
    string totalData = "+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, CrInSecondMessage_Success)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string totalData1 = totalData.substr(0, totalData.length() / 2);
    string totalData2 = totalData.substr(totalData1.length());
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData1, tickLimit / 2);
    this->EnqueueMessage(totalData2, tickLimit / 2);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, InsufficentSearchBuffer_Fail)
{
    const char *needle = "\n";
    string expData = "+COPS:0,2,3,4";
    char actData[expData.length() + 1];
    size_t actDataLen = expData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES), rc);
    ASSERT_EQ(expData.length(), actDataLen);
    actData[actDataLen] = '\0';
    ASSERT_EQ(0, strncmp(expData.c_str(), actData, actDataLen));
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, PeekLengthZero_Fail)
{
    const char *needle = "\n";
    string expData = "+COPS:0,2,3,4";
    char actData[expData.length() + 1];
    size_t actDataLen = 0;
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, NULL);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES), rc);
    ASSERT_EQ(0U, actDataLen);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 1000;
    const char *needle = "\n";
    string expData = "+COPS:0,2,3,4\r\n";
    char actData[expData.length() + 1];
    size_t actDataLen = expData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(expData, tickLimit * 2);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(0U, actDataLen);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

TEST_F(TS_PeekUntil, TwoMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    string totalData = "+COPS:0,2,3,4\r\n";
    string totalData1 = totalData.substr(0, totalData.length() / 2);
    string totalData2 = totalData.substr(totalData.length() / 2);
    string expData = totalData.substr(0, totalData.find(needle));
    char actData[totalData.length() + 1];
    size_t actDataLen = totalData.length();
    memset(actData, 0U, sizeof(actData));
    this->EnqueueMessage(totalData1, tickLimit);
    this->EnqueueMessage(totalData2, 100);

    Retcode_T rc = PeekUntil(&t, actData, &actDataLen, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(totalData1.length(), actDataLen);
    ASSERT_EQ(this->GetTotalDequeuedByteCount(), this->GetFakeRxByteCount());
}

class TS_SkipUntil : public TS_FakeRx
{
};

TEST_F(TS_SkipUntil, CrNearEndOfString_Success)
{
    const char *needle = "\r";
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, LfEndOfString_Success)
{
    const char *needle = "\n";
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, CrOrLfNearEndOfString_Success)
{
    const char *needle = "\r\n";
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, CrInSecondMessage_Success)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *totalData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    this->EnqueueMessage(totalData1, expDataLen1, tickLimit / 2);
    this->EnqueueMessage(expData2, expDataLen2, tickLimit / 2);

    Retcode_T rc = SkipUntil(&t, needle, &tickLimit);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, LfStartOfString_Success)
{
    const char *needle = "\n";
    const char *expData = "\n+COPS:0,2,3,4";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, LfStartOfStringIgnoreMultipleOccurrences_Success)
{
    const char *needle = "\n";
    const char *expData = "\n+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, LfOnlyCharInString_Success)
{
    const char *needle = "\n";
    const char *expData = "\n";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen);

    Retcode_T rc = SkipUntil(&t, needle, NULL);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_SkipUntil, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 1000;
    const char *needle = "\n";
    const char *expData = "+COPS:0,2,3,4";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen, tickLimit * 2);

    Retcode_T rc = SkipUntil(&t, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

TEST_F(TS_SkipUntil, TwoMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    const char *needle = "\r";
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *totalData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    this->EnqueueMessage(totalData1, expDataLen1, tickLimit);
    this->EnqueueMessage(expData2, expDataLen2, 100);

    Retcode_T rc = SkipUntil(&t, needle, &tickLimit);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

class TS_Skip : public TS_FakeRx
{
};

TEST_F(TS_Skip, OneMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4";
    size_t expDataLen = strlen(expData);
    size_t expAmount = expDataLen / 2;
    this->EnqueueMessage(expData, expDataLen);

    size_t r = Skip(&t, expAmount, NULL);

    EXPECT_EQ(expAmount, r);
    EXPECT_EQ(expDataLen - expAmount, this->GetFakeRxByteCount());
}

TEST_F(TS_Skip, ZeroBytes_Success)
{
    const char *expData = "+COPS:0,2,3,4";
    size_t expDataLen = strlen(expData);
    size_t expAmount = 0;
    this->EnqueueMessage(expData, expDataLen);

    size_t r = Skip(&t, expAmount, NULL);

    EXPECT_EQ(expAmount, r);
    EXPECT_EQ(expDataLen - expAmount, this->GetFakeRxByteCount());
}

TEST_F(TS_Skip, TwoMsg_Success)
{
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    size_t expAmount = expDataLen;
    const char *totalData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    this->EnqueueMessage(totalData1, expDataLen1);
    this->EnqueueMessage(expData2, expDataLen2);

    size_t r = Skip(&t, expAmount, NULL);

    EXPECT_EQ(expAmount, r);
    EXPECT_EQ(expDataLen - expAmount, this->GetFakeRxByteCount());
}

TEST_F(TS_Skip, OneMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 1000;
    const char *expData = "+COPS:0,2,3,4";
    size_t expDataLen = strlen(expData);
    this->EnqueueMessage(expData, expDataLen, tickLimit * 2);

    size_t r = Skip(&t, expDataLen, &tickLimit);

    ASSERT_EQ(0U, r);
}

TEST_F(TS_Skip, TwoMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 10;
    const char *expData = "+COPS:0,2,3,4\r\n";
    size_t expDataLen = strlen(expData);
    const char *totalData1 = expData;
    size_t expDataLen1 = expDataLen / 2;
    const char *expData2 = expData + expDataLen1;
    size_t expDataLen2 = expDataLen - expDataLen1;
    this->EnqueueMessage(totalData1, expDataLen1, tickLimit);
    this->EnqueueMessage(expData2, expDataLen2, 100);

    size_t r = Skip(&t, expDataLen, &tickLimit);

    ASSERT_EQ(expDataLen1, r);
}

class TS_AtTransceiver_ReadCommandAny : public TS_FakeRx
{
};

TEST_F(TS_AtTransceiver_ReadCommandAny, LargeBuffer_Success)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    const char *expData = strstr(totalData, "+") + 1;
    size_t expDataLen = strstr(expData, ":") - expData;
    char actData[totalDataLen + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(expDataLen, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, TwoMsg_Success)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);

    const char *expData = strstr(totalData, "+") + 1;
    size_t expDataLen = strstr(totalData, ":") - expData;
    const char *totalData1 = totalData;
    size_t totalDataLen1 = totalDataLen / 2;
    const char *totalData2 = totalData + totalDataLen1;
    size_t totalDataLen2 = totalDataLen - totalDataLen1;
    char actData[totalDataLen + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData1, totalDataLen1);
    this->EnqueueMessage(totalData2, totalDataLen2);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(expDataLen, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, ProperZeroTermination_Success)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    const char *expData = strstr(totalData, "+") + 1;
    size_t expDataLen = strstr(expData, ":") - expData;
    char actData[expDataLen + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(expDataLen, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, CantFitZeroTerminator_Warn)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    const char *expData = strstr(totalData, "+") + 1;
    size_t expDataLen = strstr(expData, ":") - expData;
    char actData[expDataLen];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES), rc);
    EXPECT_EQ(expDataLen - 1, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, BufferTooSmall_Warn)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    const char *expData = strstr(totalData, "+") + 1;
    size_t expDataLen = strstr(expData, ":") - expData;
    char actData[expDataLen / 2 + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES), rc);
    EXPECT_EQ(expDataLen / 2, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, BufferCanOnlyHoldZeroTerminator_Warn)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    char actData[1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES), rc);
    EXPECT_EQ(0U, strnlen(actData, sizeof(actData)));
}

TEST_F(TS_AtTransceiver_ReadCommandAny, EmptyBuffer_Warn)
{
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    char actData[0];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES), rc);
}

TEST_F(TS_AtTransceiver_ReadCommandAny, InvalidCommandInitiator_Fail)
{
    const char *totalData = "\r\n?COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    char actData[totalDataLen + 1];
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

TEST_F(TS_AtTransceiver_ReadCommandAny, NoCommandInRingBuffer_Fail)
{
    const char *totalData = "";
    size_t totalDataLen = strlen(totalData);
    char actData[totalDataLen + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData, totalDataLen);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

TEST_F(TS_AtTransceiver_ReadCommandAny, TwoMsgWithTimeout_Fail)
{
    TickType_t tickLimit = 100;
    const char *totalData = "\r\n+COPS:0,2,3,4\r\n";
    size_t totalDataLen = strlen(totalData);
    const char *totalData1 = totalData;
    size_t totalDataLen1 = strstr(totalData, ":") - totalData;
    const char *totalData2 = totalData + totalDataLen1;
    size_t totalDataLen2 = totalDataLen - totalDataLen1;
    char actData[totalDataLen + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData1, totalDataLen1, tickLimit * 2);
    this->EnqueueMessage(totalData2, totalDataLen2, 0);

    Retcode_T rc = AtTransceiver_ReadCommandAny(&t, actData, sizeof(actData), tickLimit);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

class TS_AtTransceiver_ReadCommand : public TS_FakeRx
{
};

TEST_F(TS_AtTransceiver_ReadCommand, COPS_Success)
{
    string totalData = "\r\n+COPS:0,2,3,4\r\n";
    string expData = totalData.substr(0, totalData.find(':')).substr(totalData.find('+') + 1);
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expData.c_str(), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(totalData.length() - totalData.find(':') - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_AtTransceiver_ReadCommand, CGREG_Success)
{
    string totalData = "\r\n+CGREG:1,2,3\r\n";
    string expData = totalData.substr(0, totalData.find(':')).substr(totalData.find('+') + 1);
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expData.c_str(), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(totalData.length() - totalData.find(':') - 1, this->GetFakeRxByteCount());
}

TEST_F(TS_AtTransceiver_ReadCommand, InvalidCommandInitiator_Fail)
{
    string totalData = "\r\n?CGREG:1,2,3\r\n";
    string expData = totalData.substr(0, totalData.find(':')).substr(totalData.find('+') + 1);
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expData.c_str(), 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
}

TEST_F(TS_AtTransceiver_ReadCommand, TwoMsg_Success)
{
    string totalData = "\r\n+CGREG:1,2,3\r\n";
    string totalData1 = totalData.substr(0, totalData.find(':') / 2);
    string totalData2 = totalData.substr(totalData1.length());
    string expData = totalData.substr(0, totalData.find(':')).substr(totalData.find('+') + 1);
    this->EnqueueMessage(totalData1);
    this->EnqueueMessage(totalData2);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expData.c_str(), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_EQ(totalData.length() - totalData.find(':') - 1, this->GetFakeRxByteCount());
}

class TS_AtTransceiver_ReadI32 : public TS_FakeRx
{
};
class TS_AtTransceiver_ReadI16 : public TS_FakeRx
{
};
class TS_AtTransceiver_ReadI8 : public TS_FakeRx
{
};

/* Table: NAME, BITSIZE, EXPECTED, BASE */
#define ATTRANSCEIVER_INT_TEST_TABLE(EXP)      \
    EXP(PosRand, 32, random(0, RAND_MAX), 10)  \
    EXP(NegRand, 32, random(-RAND_MAX, 0), 10) \
    EXP(Zero, 32, 0, 10)                       \
    EXP(Max, 32, INT32_MAX, 10)                \
    EXP(Min, 32, INT32_MIN, 10)                \
    EXP(PosRand, 16, random(0, RAND_MAX), 10)  \
    EXP(NegRand, 16, random(-RAND_MAX, 0), 10) \
    EXP(Zero, 16, 0, 10)                       \
    EXP(Max, 16, INT16_MAX, 10)                \
    EXP(Min, 16, INT16_MIN, 10)                \
    EXP(PosRand, 8, random(0, RAND_MAX), 10)   \
    EXP(NegRand, 8, random(-RAND_MAX, 0), 10)  \
    EXP(Zero, 8, 0, 10)                        \
    EXP(Max, 8, INT8_MAX, 10)                  \
    EXP(Min, 8, INT8_MIN, 10)                  \
                                               \
    EXP(PosRand, 32, random(0, RAND_MAX), 16)  \
    EXP(NegRand, 32, random(-RAND_MAX, 0), 16) \
    EXP(Zero, 32, 0, 16)                       \
    EXP(Max, 32, INT32_MAX, 16)                \
    EXP(Min, 32, INT32_MIN, 16)                \
    EXP(PosRand, 16, random(0, RAND_MAX), 16)  \
    EXP(NegRand, 16, random(-RAND_MAX, 0), 16) \
    EXP(Zero, 16, 0, 16)                       \
    EXP(Max, 16, INT16_MAX, 16)                \
    EXP(Min, 16, INT16_MIN, 16)                \
    EXP(PosRand, 8, random(0, RAND_MAX), 16)   \
    EXP(NegRand, 8, random(-RAND_MAX, 0), 16)  \
    EXP(Zero, 8, 0, 16)                        \
    EXP(Max, 8, INT8_MAX, 16)                  \
    EXP(Min, 8, INT8_MIN, 16)                  \
                                               \
    EXP(PosRand, 32, random(0, RAND_MAX), 8)   \
    EXP(NegRand, 32, random(-RAND_MAX, 0), 8)  \
    EXP(Zero, 32, 0, 8)                        \
    EXP(Max, 32, INT32_MAX, 8)                 \
    EXP(Min, 32, INT32_MIN, 8)                 \
    EXP(PosRand, 16, random(0, RAND_MAX), 8)   \
    EXP(NegRand, 16, random(-RAND_MAX, 0), 8)  \
    EXP(Zero, 16, 0, 8)                        \
    EXP(Max, 16, INT16_MAX, 8)                 \
    EXP(Min, 16, INT16_MIN, 8)                 \
    EXP(PosRand, 8, random(0, RAND_MAX), 8)    \
    EXP(NegRand, 8, random(-RAND_MAX, 0), 8)   \
    EXP(Zero, 8, 0, 8)                         \
    EXP(Max, 8, INT8_MAX, 8)                   \
    EXP(Min, 8, INT8_MIN, 8)

#define ATTRANSCEIVER_INT_TEST_FIXTURE_EXP(NAME, BITSIZE, EXPECTED, BASE)                    \
    TEST_F(TS_AtTransceiver_ReadI##BITSIZE, NAME##Base##BASE##MiddleParam_Generated_Success) \
    {                                                                                        \
        int##BITSIZE##_t expX = EXPECTED;                                                    \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (int32_t)expX << ",2,3\r\n";                                                   \
        const string &s = ss.str();                                                          \
        this->EnqueueMessage(s);                                                             \
        int##BITSIZE##_t x = !expX; /* make sure x and expX are different */                 \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadI##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }                                                                                        \
                                                                                             \
    TEST_F(TS_AtTransceiver_ReadI##BITSIZE, NAME##Base##BASE##LastParam_Generated_Success)   \
    {                                                                                        \
        int##BITSIZE##_t expX = EXPECTED;                                                    \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (int32_t)expX << "\r\n";                                                       \
        const string &s = ss.str();                                                          \
        this->EnqueueMessage(s);                                                             \
        int##BITSIZE##_t x = !expX; /* make sure x and expX are different */                 \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadI##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }                                                                                        \
                                                                                             \
    TEST_F(TS_AtTransceiver_ReadI##BITSIZE, NAME##Base##BASE##TwoMsg_Generated_Success)      \
    {                                                                                        \
        int##BITSIZE##_t expX = EXPECTED;                                                    \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (int32_t)expX << "\r\n";                                                       \
        const string &s = ss.str();                                                          \
        const string &s1 = s.substr(0, s.length() / 2);                                      \
        const string &s2 = s.substr(s.length() / 2);                                         \
        this->EnqueueMessage(s1);                                                            \
        this->EnqueueMessage(s2);                                                            \
        int##BITSIZE##_t x = !expX; /* make sure x and expX are different */                 \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadI##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }

ATTRANSCEIVER_INT_TEST_TABLE(ATTRANSCEIVER_INT_TEST_FIXTURE_EXP)

TEST_F(TS_AtTransceiver_ReadI32, NegativeZero_Success)
{
    int32_t expX = 0;
    stringstream ss;
    ss << "-" << expX << ",2,3\r\n";
    const string &s = ss.str();
    this->EnqueueMessage(s);
    int32_t x = !expX; // make sure x and expX are different

    Retcode_T rc = AtTransceiver_ReadI32(&t, &x, 10, 0);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expX, x);
}

TEST_F(TS_AtTransceiver_ReadI32, NoDelimiter_Fail)
{
    int32_t expX = random(0, RAND_MAX);
    stringstream ss;
    ss << expX << "2";
    const string &s = ss.str();
    this->EnqueueMessage(s);
    int32_t x = !expX; // make sure x and expX are different

    Retcode_T rc = AtTransceiver_ReadI32(&t, &x, 10, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_NE(expX, x);
}

TEST_F(TS_AtTransceiver_ReadI32, InvalidString_Fail)
{
    int32_t expX = 0;
    stringstream ss;
    ss << "abcd,123\r\n";
    const string &s = ss.str();
    this->EnqueueMessage(s);
    int32_t x = !expX; // make sure x and expX are different

    Retcode_T rc = AtTransceiver_ReadI32(&t, &x, 10, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_NE(expX, x);
}

class TS_AtTransceiver_ReadU32 : public TS_FakeRx
{
};
class TS_AtTransceiver_ReadU16 : public TS_FakeRx
{
};
class TS_AtTransceiver_ReadU8 : public TS_FakeRx
{
};

/* Table: NAME, BITSIZE, EXPECTED, BASE */
#define ATTRANSCEIVER_UINT_TEST_TABLE(EXP)    \
    EXP(PosRand, 32, random(0, RAND_MAX), 10) \
    EXP(Zero, 32, 0, 10)                      \
    EXP(Max, 32, UINT32_MAX, 10)              \
    EXP(PosRand, 16, random(0, RAND_MAX), 10) \
    EXP(Zero, 16, 0, 10)                      \
    EXP(Max, 16, UINT16_MAX, 10)              \
    EXP(PosRand, 8, random(0, RAND_MAX), 10)  \
    EXP(Zero, 8, 0, 10)                       \
    EXP(Max, 8, UINT8_MAX, 10)                \
                                              \
    EXP(PosRand, 32, random(0, RAND_MAX), 16) \
    EXP(Zero, 32, 0, 16)                      \
    EXP(Max, 32, UINT32_MAX, 16)              \
    EXP(PosRand, 16, random(0, RAND_MAX), 16) \
    EXP(Zero, 16, 0, 16)                      \
    EXP(Max, 16, UINT16_MAX, 16)              \
    EXP(PosRand, 8, random(0, RAND_MAX), 16)  \
    EXP(Zero, 8, 0, 16)                       \
    EXP(Max, 8, UINT8_MAX, 16)                \
                                              \
    EXP(PosRand, 32, random(0, RAND_MAX), 8)  \
    EXP(Zero, 32, 0, 8)                       \
    EXP(Max, 32, UINT32_MAX, 8)               \
    EXP(PosRand, 16, random(0, RAND_MAX), 8)  \
    EXP(Zero, 16, 0, 8)                       \
    EXP(Max, 16, UINT16_MAX, 8)               \
    EXP(PosRand, 8, random(0, RAND_MAX), 8)   \
    EXP(Zero, 8, 0, 8)                        \
    EXP(Max, 8, UINT8_MAX, 8)

#define ATTRANSCEIVER_UINT_TEST_FIXTURE_EXP(NAME, BITSIZE, EXPECTED, BASE)                   \
    TEST_F(TS_AtTransceiver_ReadU##BITSIZE, NAME##Base##BASE##MiddleParam_Generated_Success) \
    {                                                                                        \
        uint##BITSIZE##_t expX = EXPECTED;                                                   \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (uint32_t)expX << ",2,3\r\n";                                                  \
        const string &s = ss.str();                                                          \
        this->EnqueueMessage(s);                                                             \
        uint##BITSIZE##_t x = !expX; /* make sure x and expX are different */                \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadU##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }                                                                                        \
                                                                                             \
    TEST_F(TS_AtTransceiver_ReadU##BITSIZE, NAME##Base##BASE##LastParam_Generated_Success)   \
    {                                                                                        \
        uint##BITSIZE##_t expX = EXPECTED;                                                   \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (uint32_t)expX << "\r\n";                                                      \
        const string &s = ss.str();                                                          \
        this->EnqueueMessage(s);                                                             \
        uint##BITSIZE##_t x = !expX; /* make sure x and expX are different */                \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadU##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }                                                                                        \
                                                                                             \
    TEST_F(TS_AtTransceiver_ReadU##BITSIZE, NAME##Base##BASE##TwoMsg_Generated_Success)      \
    {                                                                                        \
        uint##BITSIZE##_t expX = EXPECTED;                                                   \
        stringstream ss;                                                                     \
        switch (BASE)                                                                        \
        {                                                                                    \
        default:                                                                             \
        case 0:                                                                              \
            ss << dec;                                                                       \
            break;                                                                           \
        case 8:                                                                              \
            ss << oct;                                                                       \
            break;                                                                           \
        case 16:                                                                             \
            ss << hex;                                                                       \
            break;                                                                           \
        }                                                                                    \
        ss << (uint32_t)expX << "\r\n";                                                      \
        const string &s = ss.str();                                                          \
        const string &s1 = s.substr(0, s.length() / 2);                                      \
        const string &s2 = s.substr(s.length() / 2);                                         \
        this->EnqueueMessage(s1);                                                            \
        this->EnqueueMessage(s2);                                                            \
        uint##BITSIZE##_t x = !expX; /* make sure x and expX are different */                \
                                                                                             \
        Retcode_T rc = AtTransceiver_ReadU##BITSIZE(&t, &x, BASE, 0);                        \
                                                                                             \
        ASSERT_EQ(RETCODE_OK, rc);                                                           \
        ASSERT_EQ(expX, x);                                                                  \
    }

ATTRANSCEIVER_UINT_TEST_TABLE(ATTRANSCEIVER_UINT_TEST_FIXTURE_EXP)

TEST_F(TS_AtTransceiver_ReadU32, NoDelimiter_Fail)
{
    uint32_t expX = random(0, RAND_MAX);
    stringstream ss;
    ss << expX << "2";
    const string &s = ss.str();
    this->EnqueueMessage(s);
    uint32_t x = !expX; // make sure x and expX are different

    Retcode_T rc = AtTransceiver_ReadU32(&t, &x, 10, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_NE(expX, x);
}

TEST_F(TS_AtTransceiver_ReadU32, InvalidString_Fail)
{
    uint32_t expX = 0;
    stringstream ss;
    ss << "abcd,123\r\n";
    const string &s = ss.str();
    this->EnqueueMessage(s);
    uint32_t x = !expX; // make sure x and expX are different

    Retcode_T rc = AtTransceiver_ReadU32(&t, &x, 10, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_NE(expX, x);
}

class TS_AtTransceiver_ReadString : public TS_FakeRx
{
};

TEST_F(TS_AtTransceiver_ReadString, LargeBuffer_Success)
{
    string totalData = "\"Hello World!\",123\r\n";
    string expData = totalData.substr(totalData.find("\"") + 1);
    expData = expData.substr(0, expData.find("\""));
    char actData[expData.length() * 2];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadString(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_STREQ(expData.c_str(), actData);
}

TEST_F(TS_AtTransceiver_ReadString, SmallBuffer_Success)
{
    string totalData = "\"Hello World!\",123\r\n";
    string expData = totalData.substr(totalData.find("\"") + 1);
    expData = expData.substr(0, expData.find("\""));
    char actData[expData.length() + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadString(&t, actData, sizeof(actData), 0);

    EXPECT_EQ(RETCODE_OK, rc);
    EXPECT_STREQ(expData.c_str(), actData);
}

TEST_F(TS_AtTransceiver_ReadString, CantFitZeroTerminator_Fail)
{
    string totalData = "\"Hello World!\",123\r\n";
    string expData = totalData.substr(totalData.find("\"") + 1);
    expData = expData.substr(0, expData.find("\""));
    char actData[expData.length() + 1];
    memset(actData, 'A', sizeof(actData));
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadString(&t, actData, sizeof(actData) - 1, 0);

    EXPECT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES), rc);
    EXPECT_EQ('A', actData[sizeof(actData) - 1]);
}

class TS_AtTransceiver_ReadHexString : public TS_FakeRx
{
};

TEST_F(TS_AtTransceiver_ReadHexString, LargeString_Success)
{
    string expData = "This should be decoded from hex!";
    stringstream ss;
    ss << "\"";
    for (const char &c : expData)
    {
        ss << hex << (uint32_t)c;
    }
    ss << "\",123\r\n";
    const string &totalData = ss.str();
    this->EnqueueMessage(totalData);

    char actData[expData.length() + 1];
    memset(actData, 'A', sizeof(actData));
    size_t numActualBytesRead = 0;
    Retcode_T rc = AtTransceiver_ReadHexString(&t, actData, sizeof(actData) - 1, &numActualBytesRead, 0);
    actData[sizeof(actData) - 1] = '\0';

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), numActualBytesRead);
    ASSERT_STREQ(expData.c_str(), actData);
}

TEST_F(TS_AtTransceiver_ReadHexString, SingleByte_Success)
{
    string expData = "A";
    stringstream ss;
    ss << "\"";
    for (const char &c : expData)
    {
        ss << hex << (uint32_t)c;
    }
    ss << "\",123\r\n";
    const string &totalData = ss.str();
    this->EnqueueMessage(totalData);

    char actData[expData.length() + 1];
    memset(actData, 'A', sizeof(actData));
    size_t numActualBytesRead = 0;
    Retcode_T rc = AtTransceiver_ReadHexString(&t, actData, sizeof(actData) - 1, &numActualBytesRead, 0);
    actData[sizeof(actData) - 1] = '\0';

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), numActualBytesRead);
    ASSERT_STREQ(expData.c_str(), actData);
}

TEST_F(TS_AtTransceiver_ReadHexString, TwoMsg_Success)
{
    string expData = "This should be decoded from hex!";
    stringstream ss;
    ss << "\"";
    for (const char &c : expData)
    {
        ss << hex << (uint32_t)c;
    }
    ss << "\",123\r\n";
    const string &totalData = ss.str();
    const string &totalData1 = totalData.substr(0, totalData.length() / 2);
    const string &totalData2 = totalData.substr(totalData1.length());
    this->EnqueueMessage(totalData1, 10);
    this->EnqueueMessage(totalData2, 10);

    char actData[expData.length() + 1];
    memset(actData, 'A', sizeof(actData));
    size_t numActualBytesRead = 0;
    Retcode_T rc = AtTransceiver_ReadHexString(&t, actData, sizeof(actData) - 1, &numActualBytesRead, 20);
    actData[sizeof(actData) - 1] = '\0';

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), numActualBytesRead);
    ASSERT_STREQ(expData.c_str(), actData);
}

class TS_AtTransceiver_ReadCode : public TS_FakeRx
{
};

TEST_F(TS_AtTransceiver_ReadCode, AllResponseCodes_Success)
{
    for (size_t i = 0; i < ATTRANSCEIVER_RESPONSECODE_MAX; ++i)
    {
        enum AtTransceiver_ResponseCode_E expCode = (enum AtTransceiver_ResponseCode_E)i;
        stringstream ss;
        ss << "\r\n"
           << AtTransceiver_ResponseCodeAsString(expCode);
        if (expCode == ATTRANSCEIVER_RESPONSECODE_CONNECTDR)
            ss << "115200";
        ss << "\r\n";
        string totalData = ss.str();
        cout << "Testing " << totalData;
        enum AtTransceiver_ResponseCode_E actCode;
        this->EnqueueMessage(totalData);

        Retcode_T rc = AtTransceiver_ReadCode(&t, &actCode, 0);

        ASSERT_EQ(RETCODE_OK, rc);
        ASSERT_EQ(expCode, actCode);
    }
}

class TS_ParseFullResponse : public TS_FakeRx
{
};

TEST_F(TS_ParseFullResponse, CommandResponse_Success)
{
    string expCmd = "COPS";
    uint32_t expAttrb1 = 123;
    string expAttrb2 = "Hello World";
    int32_t expAttrb3 = -123;
    string expAttrb4 = "this should be skipped";
    uint8_t expAttrb5 = 127;

    enum AtTransceiver_ResponseCode_E expCode = ATTRANSCEIVER_RESPONSECODE_OK;

    stringstream ss;
    ss << "\r\n+" << expCmd << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n\r\n"
       << ResponseCodes[ATTRANSCEIVER_RESPONSECODE_OK].Verbose << "\r\n";
    string totalData = ss.str();
    cout << "Test-response: " << totalData;
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expCmd.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint32_t x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    char actAttrib2[expAttrb2.length() + 1];
    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    int32_t y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint8_t z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);

    enum AtTransceiver_ResponseCode_E actCode;
    rc = AtTransceiver_ReadCode(&t, &actCode, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expCode, actCode);
}

TEST_F(TS_ParseFullResponse, OneUnsolicitedResponse_Success)
{
    string expCmd = "CEREG";
    uint32_t expAttrb1 = 123;
    string expAttrb2 = "Hello World";
    int32_t expAttrb3 = -123;
    string expAttrb4 = "this should be skipped";
    uint8_t expAttrb5 = 127;

    stringstream ss;
    ss << "\r\n+" << expCmd << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n";
    string totalData = ss.str();
    cout << "Test-response: " << totalData;
    this->EnqueueMessage(totalData);

    Retcode_T rc = AtTransceiver_ReadCommand(&t, expCmd.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint32_t x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    char actAttrib2[expAttrb2.length() + 1];
    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    int32_t y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint8_t z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);
}

TEST_F(TS_ParseFullResponse, TwoUnsolicitedResponses_Success)
{
    string expCmd1 = "CEREG";
    string expCmd2 = "CGREG";
    uint32_t expAttrb1 = 123;
    string expAttrb2 = "Hello World";
    int32_t expAttrb3 = -123;
    string expAttrb4 = "this should be skipped";
    uint8_t expAttrb5 = 127;

    stringstream ss;
    ss << dec << "\r\n+" << expCmd1 << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n";
    ss << dec << "\r\n+" << expCmd2 << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n";
    string totalData = ss.str();
    cout << "Test-response: " << totalData;
    this->EnqueueMessage(totalData);

    /* first response */
    Retcode_T rc = AtTransceiver_ReadCommand(&t, expCmd1.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint32_t x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    char actAttrib2[expAttrb2.length() + 1];
    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    int32_t y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint8_t z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);

    /* second response */
    rc = AtTransceiver_ReadCommand(&t, expCmd2.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);
}

TEST_F(TS_ParseFullResponse, TwoNaughtyUnsolicitedResponses_Success)
{
    string expCmd1 = "CEREG";
    string expCmd2 = "CGREG";
    uint32_t expAttrb1 = 123;
    string expAttrb2 = "Hello World";
    int32_t expAttrb3 = -123;
    string expAttrb4 = "this should be skipped";
    uint8_t expAttrb5 = 127;

    stringstream ss;
    ss << dec << "\r\n+" << expCmd1 << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n";
    ss << dec << "\r\n\r\n\r\n+" << expCmd2 << ":" << expAttrb1 << ","
       << "\"" << expAttrb2 << "\"," << expAttrb3 << ",\"" << expAttrb4 << "\"," << hex << (uint32_t)expAttrb5 << "\r\n";
    /* We've added a few emtpy lines between the two responses. This was
     * observed on ublox SARA R410. When connected to the tower and having CGREG
     * and CEREG URCs enabled (i.e. +CEREG:1,1 and +CGREG:1,1). If we then
     * execute AT+COPS=2 (i.e. deregister from network), the modem will return a
     * slightly malformed two responses as seen above.
     */
    string totalData = ss.str();
    cout << "Test-response: " << totalData;
    this->EnqueueMessage(totalData);

    /* first response */
    Retcode_T rc = AtTransceiver_ReadCommand(&t, expCmd1.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint32_t x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    char actAttrib2[expAttrb2.length() + 1];
    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    int32_t y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    uint8_t z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);

    /* second response */
    rc = AtTransceiver_ReadCommand(&t, expCmd2.c_str(), 0);
    ASSERT_EQ(RETCODE_OK, rc);

    x = 0;
    rc = AtTransceiver_ReadU32(&t, &x, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb1, x);

    memset(actAttrib2, 'A', sizeof(actAttrib2));
    rc = AtTransceiver_ReadString(&t, actAttrib2, sizeof(actAttrib2), 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_STREQ(expAttrb2.c_str(), actAttrib2);

    y = 0;
    rc = AtTransceiver_ReadI32(&t, &y, 0, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb3, y);

    rc = AtTransceiver_SkipArgument(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    z = 0;
    rc = AtTransceiver_ReadU8(&t, &z, 16, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expAttrb5, z);
}

namespace F_Write
{
FAKE_VALUE_FUNC(Retcode_T, Fake, const void *, size_t, size_t *);

std::stringstream Accumulated;

Retcode_T CustomFake(const void *data, size_t length, size_t *numBytesWritten)
{
    for (size_t i = 0; i < length; ++i)
    {
        Accumulated << reinterpret_cast<const char *>(data)[i];
    }
    if (numBytesWritten)
        *numBytesWritten = length;
    return Fake_fake.return_val;
}
} // namespace F_Write

class TS_FakeTx : public testing::Test
{
protected:
    void SetUp() override
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(F_Write::Fake);
        F_Write::Accumulated.str("");
        F_Write::Accumulated.clear();

        F_Write::Fake_fake.custom_fake = F_Write::CustomFake;
    }

    std::string GetWritten()
    {
        return F_Write::Accumulated.str();
    }
};

struct FakeAtTransceiver : public AtTransceiver_S
{
public:
    FakeAtTransceiver(enum AtTransceiver_WriteOption_E writeOptions, size_t capacity = 0)
        : AtTransceiver_S::AtTransceiver_S()
    {
        WriteOptions = writeOptions;
        if (!(WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOBUFFER))
        {
            TxBuffer = calloc(capacity, sizeof(char));
            TxBufferLength = capacity;
        }
        Write = F_Write::Fake;
        WriteState = ATTRANSCEIVER_WRITESTATE_START;
    }
    ~FakeAtTransceiver()
    {
        if (TxBuffer)
        {
            free(TxBuffer);
            TxBuffer = nullptr;
        }
    }
};

class TS_AtTransceiver_Write : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_Write, Default_Success)
{
    std::string data("AT+USOWR=1,11,HELLO WORLD");
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, data.length() + 1);
    enum AtTransceiver_WriteState_E newWriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_Write(&t, data.c_str(), data.length(), newWriteState);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(newWriteState, t.WriteState);
    ASSERT_EQ(data.length(), t.TxBufferUsed);
    ASSERT_STREQ(data.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Write, NoBuffer_Success)
{
    std::string data("AT+USOWR=1,11,HELLO WORLD\r\n");
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_NOBUFFER, data.length() + 1);
    enum AtTransceiver_WriteState_E newWriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_Write(&t, data.c_str(), data.length(), newWriteState);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(newWriteState, t.WriteState);
    ASSERT_GT(F_Write::Fake_fake.call_count, 0U);
    ASSERT_STREQ(data.c_str(), GetWritten().c_str());
}

TEST_F(TS_AtTransceiver_Write, NoState_Success)
{
    std::string data("AT+USOWR=1,11,HELLO WORLD\r\n");
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_NOSTATE, data.length() + 1);
    enum AtTransceiver_WriteState_E newWriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_Write(&t, data.c_str(), data.length(), newWriteState);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_START, t.WriteState);
    ASSERT_EQ(data.length(), t.TxBufferUsed);
    ASSERT_STREQ(data.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Write, DefaultTxBufferTooSmall_Fail)
{
    std::string data("AT+USOWR=1,11,HELLO WORLD\r\n");
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, data.length() / 2);
    enum AtTransceiver_WriteState_E newWriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_Write(&t, data.c_str(), data.length(), newWriteState);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES), rc);
    ASSERT_EQ(newWriteState, t.WriteState);
    ASSERT_EQ(t.TxBufferLength, t.TxBufferUsed);
}

class TS_AtTransceiver_WriteAction : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteAction, PrefixATCGMM_Pass)
{
    std::string action("+CGMM");
    std::stringstream ss;
    ss << "AT" << action;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteAction(&t, action.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteAction, NoMnemonicAT_Pass)
{
    std::string action("");
    std::stringstream ss;
    ss << "AT";
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteAction(&t, action.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteAction, NoPrefixATE_Pass)
{
    std::string action("E");
    std::stringstream ss;
    ss << "ATE";
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteAction(&t, action.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteAction, NoState_Pass)
{
    std::string action("E");
    std::stringstream ss;
    ss << "ATE";
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteOptions = ATTRANSCEIVER_WRITEOPTION_NOSTATE;
    t.WriteState = ATTRANSCEIVER_WRITESTATE_INVALID;

    Retcode_T rc = AtTransceiver_WriteAction(&t, action.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_INVALID, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteAction, WrongState_Fail)
{
    std::string action("+CGMR");
    std::stringstream ss;
    ss << "AT" << action;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteAction(&t, action.c_str());

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
}

class TS_AtTransceiver_WriteSet : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteSet, PrefixATCOPS_Pass)
{
    std::string set("+COPS");
    std::stringstream ss;
    ss << "AT" << set << '=';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteSet(&t, set.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_COMMAND, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteSet, NoPrefixATS4_Pass)
{
    std::string set("S4");
    std::stringstream ss;
    ss << "AT" << set << '=';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteSet(&t, set.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_COMMAND, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteSet, WrongState_Fail)
{
    std::string set("+CGCONT");
    std::stringstream ss;
    ss << "AT" << set << '=';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteSet(&t, set.c_str());

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
}

class TS_AtTransceiver_WriteGet : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteGet, PrefixATCGPADDR_Pass)
{
    std::string get("+CGPADDR");
    std::stringstream ss;
    ss << "AT" << get << '?';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteGet(&t, get.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteGet, NoPrefixATS3_Pass)
{
    std::string get("S3");
    std::stringstream ss;
    ss << "AT" << get << '?';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());

    Retcode_T rc = AtTransceiver_WriteGet(&t, get.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteGet, WrongState_Fail)
{
    std::string get("+CGCONT");
    std::stringstream ss;
    ss << "AT" << get << '?';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteGet(&t, get.c_str());

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
}

class PTS_AtTransceiver_WriteI32 : public testing::TestWithParam<int32_t>
{
};

TEST_P(PTS_AtTransceiver_WriteI32, Dec_Pass)
{
    int32_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI32, DecArgState_Pass)
{
    int32_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI32, Oct_Pass)
{
    int32_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI32, Hex_Pass)
{
    int32_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteI32,
                        testing::Values(
                            0,
                            1, 2, 3,
                            -1, -2, -3,
                            INT32_MAX, INT32_MIN,
                            INT32_MAX / 2, INT32_MIN / 2,
                            INT16_MAX, INT16_MIN,
                            INT8_MAX, INT8_MIN));

class PTS_AtTransceiver_WriteI16 : public testing::TestWithParam<int16_t>
{
};

TEST_P(PTS_AtTransceiver_WriteI16, Dec_Pass)
{
    int16_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI16(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI16, DecArgState_Pass)
{
    int16_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteI16(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI16, Oct_Pass)
{
    int16_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI16(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI16, Hex_Pass)
{
    int16_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI16(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteI16,
                        testing::Values(
                            0,
                            1, 2, 3,
                            -1, -2, -3,
                            INT16_MAX, INT16_MIN,
                            INT16_MAX / 2, INT16_MIN / 2,
                            INT8_MAX, INT8_MIN));

class PTS_AtTransceiver_WriteI8 : public testing::TestWithParam<int8_t>
{
};

TEST_P(PTS_AtTransceiver_WriteI8, Dec_Pass)
{
    int8_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI8(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI8, DecArgState_Pass)
{
    int8_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteI8(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI8, Oct_Pass)
{
    int8_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI8(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteI8, Hex_Pass)
{
    int8_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << (int32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteI8(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteI8,
                        testing::Values(
                            0,
                            1, 2, 3,
                            -1, -2, -3,
                            INT8_MAX, INT8_MIN,
                            INT8_MAX / 2, INT8_MIN / 2));

class PTS_AtTransceiver_WriteU32 : public testing::TestWithParam<uint32_t>
{
};

TEST_P(PTS_AtTransceiver_WriteU32, Dec_Pass)
{
    uint32_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU32, DecArgState_Pass)
{
    uint32_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU32, Oct_Pass)
{
    uint32_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU32, Hex_Pass)
{
    uint32_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteU32,
                        testing::Values(
                            0,
                            1, 2, 3,
                            INT32_MAX,
                            INT32_MAX / 2,
                            INT16_MAX,
                            INT8_MAX));

class PTS_AtTransceiver_WriteU16 : public testing::TestWithParam<uint16_t>
{
};

TEST_P(PTS_AtTransceiver_WriteU16, Dec_Pass)
{
    uint16_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU16(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU16, DecArgState_Pass)
{
    uint16_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteU16(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU16, Oct_Pass)
{
    uint16_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU16(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU16, Hex_Pass)
{
    uint16_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU16(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteU16,
                        testing::Values(
                            0,
                            1, 2, 3,
                            UINT16_MAX,
                            UINT16_MAX / 2,
                            UINT8_MAX));

class PTS_AtTransceiver_WriteU8 : public testing::TestWithParam<uint8_t>
{
};

TEST_P(PTS_AtTransceiver_WriteU8, Dec_Pass)
{
    uint8_t x = GetParam();
    std::stringstream ss;
    ss << std::dec << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU8(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU8, DecArgState_Pass)
{
    uint8_t x = GetParam();
    std::stringstream ss;
    ss << ',' << std::dec << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteU8(&t, x, 10);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU8, Oct_Pass)
{
    uint8_t x = GetParam();
    std::stringstream ss;
    ss << std::oct << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU8(&t, x, 8);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_P(PTS_AtTransceiver_WriteU8, Hex_Pass)
{
    uint8_t x = GetParam();
    std::stringstream ss;
    ss << std::hex << (uint32_t)x;
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteU8(&t, x, 16);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

INSTANTIATE_TEST_CASE_P(Various, PTS_AtTransceiver_WriteU8,
                        testing::Values(
                            0,
                            1, 2, 3,
                            UINT8_MAX,
                            UINT8_MAX / 2));

class TS_AtTransceiver_WriteI32 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteI32, WrongState_Fail)
{
    int32_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

TEST_F(TS_AtTransceiver_WriteI32, InvalidBase_Fail)
{
    int32_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteI32(&t, x, 123);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM), rc);
}

class TS_AtTransceiver_WriteI16 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteI16, WrongState_Fail)
{
    int16_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteI16(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_WriteI8 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteI8, WrongState_Fail)
{
    int8_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteI8(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_WriteU32 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteU32, WrongState_Fail)
{
    uint32_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

TEST_F(TS_AtTransceiver_WriteU32, InvalidBase_Fail)
{
    uint32_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteU32(&t, x, 123);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM), rc);
}

class TS_AtTransceiver_WriteU16 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteU16, WrongState_Fail)
{
    uint16_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteU16(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_WriteU8 : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteU8, WrongState_Fail)
{
    uint8_t x = 123;
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteU8(&t, x, 10);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_WriteString : public TS_FakeTx
{
};

TEST_F(TS_AtTransceiver_WriteString, CmdState_Pass)
{
    std::string str("hello");
    std::stringstream ss;
    ss << '"' << str << '"';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteString(&t, str.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteString, ArgState_Pass)
{
    std::string str("hello");
    std::stringstream ss;
    ss << ',' << '"' << str << '"';
    const std::string &expData = ss.str();
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteString(&t, str.c_str());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteString, WrongState_Fail)
{
    const char *str = "hello";
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteString(&t, str);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_WriteHexString : public TS_FakeTx
{
protected:
    std::string ToHexStr(const void *data, size_t length)
    {
        std::stringstream ss;
        ss << '"';
        for (size_t i = 0; i < length; ++i)
        {
            ss << std::hex << (uint32_t)((const uint8_t *)data)[i];
        }
        ss << '"';
        return ss.str();
    }
};

TEST_F(TS_AtTransceiver_WriteHexString, CmdState_Pass)
{
    std::string str("hello");
    const std::string &expData = ToHexStr(str.c_str(), str.length());
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_COMMAND;

    Retcode_T rc = AtTransceiver_WriteHexString(&t, str.c_str(), str.length());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STRCASEEQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteHexString, ArgState_Pass)
{
    std::string str("hello");
    const std::string &expData = "," + ToHexStr(str.c_str(), str.length());
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, expData.length());
    t.WriteState = ATTRANSCEIVER_WRITESTATE_ARGUMENT;

    Retcode_T rc = AtTransceiver_WriteHexString(&t, str.c_str(), str.length());

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STRCASEEQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_WriteHexString, WrongState_Fail)
{
    const char *str = "hello";
    FakeAtTransceiver t(ATTRANSCEIVER_WRITEOPTION_DEFAULT, 100);
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;

    Retcode_T rc = AtTransceiver_WriteHexString(&t, str, strlen(str));

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
}

class TS_AtTransceiver_Flush : public TS_FakeRx
{
protected:
    virtual void SetUp() override
    {
        TS_FakeRx::SetUp();

        RESET_FAKE(F_Write::Fake);
        F_Write::Accumulated.str("");
        F_Write::Accumulated.clear();

        F_Write::Fake_fake.custom_fake = F_Write::CustomFake;

        t.WriteOptions = ATTRANSCEIVER_WRITEOPTION_NONE;
        t.WriteState = ATTRANSCEIVER_WRITESTATE_INVALID;
        t.TxBuffer = nullptr;
        t.TxBufferLength = 0;
        t.TxBufferUsed = 0;
    }

    std::string GetWritten()
    {
        return F_Write::Accumulated.str();
    }
};

TEST_F(TS_AtTransceiver_Flush, WithS3S4AndEcho_Pass)
{
    std::string command = "AT+COPS?";
    std::stringstream ss;
    ss << command << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    char txData[expData.length() + 1];
    strncpy(txData, command.c_str(), sizeof(txData));
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;
    t.TxBuffer = txData;
    t.TxBufferLength = sizeof(txData) - 1;
    t.TxBufferUsed = command.length();
    t.Write = F_Write::Fake;

    EnqueueMessage(expData);

    Retcode_T rc = AtTransceiver_Flush(&t, 0);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Flush, WithEchoNoS3S4_Pass)
{
    std::string command = "AT+COPS?";
    std::stringstream ss;
    ss << command;
    const std::string &expData = ss.str();
    char txData[expData.length() + 1];
    strncpy(txData, command.c_str(), sizeof(txData));
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;
    t.TxBuffer = txData;
    t.TxBufferLength = sizeof(txData) - 1;
    t.TxBufferUsed = command.length();
    t.WriteOptions = ATTRANSCEIVER_WRITEOPTION_NOFINALS3S4;
    t.Write = F_Write::Fake;

    EnqueueMessage(expData);

    Retcode_T rc = AtTransceiver_Flush(&t, 0);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Flush, NoS3S4AndEcho_Pass)
{
    std::string command = "AT+COPS?";
    std::stringstream ss;
    ss << command;
    const std::string &expData = ss.str();
    char txData[expData.length() + 1];
    strncpy(txData, command.c_str(), sizeof(txData));
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;
    t.TxBuffer = txData;
    t.TxBufferLength = sizeof(txData) - 1;
    t.TxBufferUsed = command.length();
    t.WriteOptions = (enum AtTransceiver_WriteOption_E)((int)ATTRANSCEIVER_WRITEOPTION_NOFINALS3S4 | (int)ATTRANSCEIVER_WRITEOPTION_NOECHO);
    t.Write = F_Write::Fake;

    Retcode_T rc = AtTransceiver_Flush(&t, 0);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Flush, WrongEcho_Fail)
{
    std::string command = "AT+COPS?";
    std::stringstream ss;
    ss << "BT" << command.substr(2) << ATTRANSCEIVER_S3S4; /* BT instead of AT (wrong echo) */
    const std::string &expData = ss.str();
    char txData[expData.length() + 1];
    strncpy(txData, command.c_str(), sizeof(txData));
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;
    t.TxBuffer = txData;
    t.TxBufferLength = sizeof(txData) - 1;
    t.TxBufferUsed = command.length();
    t.Write = F_Write::Fake;

    EnqueueMessage(expData);

    Retcode_T rc = AtTransceiver_Flush(&t, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE), rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STRNE(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_AtTransceiver_Flush, Timeout_Fail)
{
    std::string command = "AT+COPS?";
    std::stringstream ss;
    ss << "BT" << command.substr(2) << ATTRANSCEIVER_S3S4; /* BT instead of AT (wrong echo) */
    const std::string &expData = ss.str();
    char txData[expData.length() + 1];
    strncpy(txData, command.c_str(), sizeof(txData));
    t.WriteState = ATTRANSCEIVER_WRITESTATE_END;
    t.TxBuffer = txData;
    t.TxBufferLength = sizeof(txData) - 1;
    t.TxBufferUsed = command.length();
    t.Write = F_Write::Fake;

    EnqueueMessage(expData, 100);

    Retcode_T rc = AtTransceiver_Flush(&t, 0);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(expData.length(), t.TxBufferUsed);
    ASSERT_STRNE(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

class TS_AtTransceiver_PrepareWrite : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();
    }
};

TEST_F(TS_AtTransceiver_PrepareWrite, Default_Pass)
{
    struct AtTransceiver_S t;
    t.TxBufferUsed = 123;
    t.WriteState = ATTRANSCEIVER_WRITESTATE_INVALID;
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[100];
    size_t txBufferLen = sizeof(txBuffer);

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, txBufferLen);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(options, t.WriteOptions);
    ASSERT_EQ(txBuffer, t.TxBuffer);
    ASSERT_EQ(txBufferLen, t.TxBufferLength);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_START, t.WriteState);
}

TEST_F(TS_AtTransceiver_PrepareWrite, NoBuffer_Pass)
{
    struct AtTransceiver_S t;
    t.TxBufferUsed = 123;
    t.TxBuffer = nullptr;
    t.TxBufferLength = 0;
    t.WriteState = ATTRANSCEIVER_WRITESTATE_INVALID;
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOBUFFER;
    char txBuffer[100];
    size_t txBufferLen = sizeof(txBuffer);

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, txBufferLen);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(options, t.WriteOptions);
    ASSERT_NE(txBuffer, t.TxBuffer);
    ASSERT_NE(txBufferLen, t.TxBufferLength);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_START, t.WriteState);
}

TEST_F(TS_AtTransceiver_PrepareWrite, NoState_Pass)
{
    struct AtTransceiver_S t;
    t.TxBufferUsed = 123;
    t.WriteState = ATTRANSCEIVER_WRITESTATE_INVALID;
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOSTATE;
    char txBuffer[100];
    size_t txBufferLen = sizeof(txBuffer);

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, txBufferLen);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(options, t.WriteOptions);
    ASSERT_EQ(txBuffer, t.TxBuffer);
    ASSERT_EQ(txBufferLen, t.TxBufferLength);
    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_INVALID, t.WriteState);
}

class TS_AtTransceiver_Lock : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(xSemaphoreTake);
    }
};

TEST_F(TS_AtTransceiver_Lock, Pass)
{
    struct AtTransceiver_S t;
    t.LockHandle = (SemaphoreHandle_t)123;

    Retcode_T rc = AtTransceiver_Lock(&t);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(1U, xSemaphoreTake_fake.call_count);
    ASSERT_EQ(t.LockHandle, xSemaphoreTake_fake.arg0_val);
    ASSERT_EQ(portMAX_DELAY, xSemaphoreTake_fake.arg1_val);
}

class TS_AtTransceiver_TryLock : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();

        RESET_FAKE(xSemaphoreTake);
    }
};

TEST_F(TS_AtTransceiver_TryLock, Pass)
{
    struct AtTransceiver_S t;
    t.LockHandle = (SemaphoreHandle_t)123;
    TickType_t timeout = 321;
    xSemaphoreTake_fake.return_val = pdPASS;

    Retcode_T rc = AtTransceiver_TryLock(&t, timeout);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(1U, xSemaphoreTake_fake.call_count);
    ASSERT_EQ(t.LockHandle, xSemaphoreTake_fake.arg0_val);
    ASSERT_EQ(timeout, xSemaphoreTake_fake.arg1_val);
}

TEST_F(TS_AtTransceiver_TryLock, Timeout_Fail)
{
    struct AtTransceiver_S t;
    t.LockHandle = (SemaphoreHandle_t)123;
    TickType_t timeout = 321;
    xSemaphoreTake_fake.return_val = pdFAIL;

    Retcode_T rc = AtTransceiver_TryLock(&t, timeout);

    ASSERT_EQ(RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT), rc);
    ASSERT_EQ(1U, xSemaphoreTake_fake.call_count);
    ASSERT_EQ(t.LockHandle, xSemaphoreTake_fake.arg0_val);
    ASSERT_EQ(timeout, xSemaphoreTake_fake.arg1_val);
}

class TS_FullWriteSequence : public TS_AtTransceiver_Flush
{
};

TEST_F(TS_FullWriteSequence, DefaultAction_Pass)
{
    const char *action = "+CGMM";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << action << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteAction(&t, action);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, NoEchoAction_Pass)
{
    const char *action = "+CGMM";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << action << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOECHO;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteAction(&t, action);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, NoBufferAction_Pass)
{
    const char *action = "+CGMM";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << action << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOBUFFER;

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, nullptr, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteAction(&t, action);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), GetWritten().c_str());
}

TEST_F(TS_FullWriteSequence, DefaultGet_Pass)
{
    const char *get = "+COPS";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << get << ATTRANSCEIVER_GETSUFFIX << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteGet(&t, get);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, NoEchoGet_Pass)
{
    const char *get = "+COPS";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << get << ATTRANSCEIVER_GETSUFFIX << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOECHO;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteGet(&t, get);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, NoBufferGet_Pass)
{
    const char *get = "+COPS";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << get << ATTRANSCEIVER_GETSUFFIX << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_NOBUFFER;

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, nullptr, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteGet(&t, get);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_END, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), GetWritten().c_str());
}

TEST_F(TS_FullWriteSequence, DefaultSetUintParam_Pass)
{
    const char *set = "+COPS";
    uint32_t param1 = 2;
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << set << ATTRANSCEIVER_SETSUFFIX << param1 << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteSet(&t, set);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_COMMAND, t.WriteState);

    rc = AtTransceiver_WriteU32(&t, param1, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, DefaultSetUintIntParam_Pass)
{
    const char *set = "+COPS";
    uint32_t param1 = 2;
    int32_t param2 = -5;
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << set << ATTRANSCEIVER_SETSUFFIX << param1 << ATTRANSCEIVER_ARGSEPARATOR << param2 << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteSet(&t, set);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_COMMAND, t.WriteState);

    rc = AtTransceiver_WriteU32(&t, param1, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);

    rc = AtTransceiver_WriteI32(&t, param2, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}

TEST_F(TS_FullWriteSequence, DefaultSetUintStringParam_Pass)
{
    const char *set = "+COPS";
    uint32_t param1 = 2;
    const char *param2 = "hello";
    std::stringstream ss;
    ss << ATTRANSCEIVER_ATTENTION << set << ATTRANSCEIVER_SETSUFFIX << param1 << ATTRANSCEIVER_ARGSEPARATOR << ATTRANSCEIVER_STRLITERAL << param2 << ATTRANSCEIVER_STRLITERAL << ATTRANSCEIVER_S3S4;
    const std::string &expData = ss.str();
    enum AtTransceiver_WriteOption_E options = ATTRANSCEIVER_WRITEOPTION_DEFAULT;
    char txBuffer[expData.length() + 1];
    memset(txBuffer, 0, sizeof(txBuffer));

    Retcode_T rc = AtTransceiver_PrepareWrite(&t, options, txBuffer, sizeof(txBuffer) - 1);
    ASSERT_EQ(RETCODE_OK, rc);

    rc = AtTransceiver_WriteSet(&t, set);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_COMMAND, t.WriteState);

    rc = AtTransceiver_WriteU32(&t, param1, 0);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);

    rc = AtTransceiver_WriteString(&t, param2);
    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(ATTRANSCEIVER_WRITESTATE_ARGUMENT, t.WriteState);

    EnqueueMessage(expData);

    rc = AtTransceiver_Flush(&t, 0);
    ASSERT_EQ(RETCODE_OK, rc);

    ASSERT_EQ(0U, t.TxBufferUsed);
    ASSERT_STREQ(expData.c_str(), reinterpret_cast<const char *>(t.TxBuffer));
}
