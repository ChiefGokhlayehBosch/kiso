/*******************************************************************************
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
 ******************************************************************************/

#include <gtest.h>
#include <queue>
#include <random>

FFF_DEFINITION_BLOCK_START
extern "C"
{
#include "Kiso_Cellular.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_AT3GPP27007

#include "Kiso_Assert_th.hh"
#include "Kiso_Retcode_th.hh"
#include "Kiso_Logging_th.hh"
#include "Kiso_RingBuffer_th.hh"
#include "FreeRTOS_th.hh"
#include "task_th.hh"
#include "semphr_th.hh"

#include "Engine_th.hh"

#include <time.h>

#undef KISO_MODULE_ID
#include "AtTransceiver.c"
#undef KISO_MODULE_ID
#include "At3Gpp27007.c"
}
FFF_DEFINITION_BLOCK_END

class TS_ExtractPdpAddress : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        FFF_RESET_HISTORY();
    }
};

TEST_F(TS_ExtractPdpAddress, IPv4_Normal_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_LeadingZeros_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.002.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_Normal_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "254.128.1.2.3.4.5.6.7.8.9.10.11.12.19.55";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(254, addr.Address.IPv6[15]);
    EXPECT_EQ(128, addr.Address.IPv6[14]);
    EXPECT_EQ(1, addr.Address.IPv6[13]);
    EXPECT_EQ(2, addr.Address.IPv6[12]);
    EXPECT_EQ(3, addr.Address.IPv6[11]);
    EXPECT_EQ(4, addr.Address.IPv6[10]);
    EXPECT_EQ(5, addr.Address.IPv6[9]);
    EXPECT_EQ(6, addr.Address.IPv6[8]);
    EXPECT_EQ(7, addr.Address.IPv6[7]);
    EXPECT_EQ(8, addr.Address.IPv6[6]);
    EXPECT_EQ(9, addr.Address.IPv6[5]);
    EXPECT_EQ(10, addr.Address.IPv6[4]);
    EXPECT_EQ(11, addr.Address.IPv6[3]);
    EXPECT_EQ(12, addr.Address.IPv6[2]);
    EXPECT_EQ(19, addr.Address.IPv6[1]);
    EXPECT_EQ(55, addr.Address.IPv6[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_LeadingZeros_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "254.128.1.02.003.4.005.6.7.8.9.10.11.12.19.55";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(254, addr.Address.IPv6[15]);
    EXPECT_EQ(128, addr.Address.IPv6[14]);
    EXPECT_EQ(1, addr.Address.IPv6[13]);
    EXPECT_EQ(2, addr.Address.IPv6[12]);
    EXPECT_EQ(3, addr.Address.IPv6[11]);
    EXPECT_EQ(4, addr.Address.IPv6[10]);
    EXPECT_EQ(5, addr.Address.IPv6[9]);
    EXPECT_EQ(6, addr.Address.IPv6[8]);
    EXPECT_EQ(7, addr.Address.IPv6[7]);
    EXPECT_EQ(8, addr.Address.IPv6[6]);
    EXPECT_EQ(9, addr.Address.IPv6[5]);
    EXPECT_EQ(10, addr.Address.IPv6[4]);
    EXPECT_EQ(11, addr.Address.IPv6[3]);
    EXPECT_EQ(12, addr.Address.IPv6[2]);
    EXPECT_EQ(19, addr.Address.IPv6[1]);
    EXPECT_EQ(55, addr.Address.IPv6[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_Quotes_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "\"192.168.2.100\"";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_Quotes_Pass)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "\"254.128.1.2.3.4.5.6.7.8.9.10.11.12.19.55\"";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(254, addr.Address.IPv6[15]);
    EXPECT_EQ(128, addr.Address.IPv6[14]);
    EXPECT_EQ(1, addr.Address.IPv6[13]);
    EXPECT_EQ(2, addr.Address.IPv6[12]);
    EXPECT_EQ(3, addr.Address.IPv6[11]);
    EXPECT_EQ(4, addr.Address.IPv6[10]);
    EXPECT_EQ(5, addr.Address.IPv6[9]);
    EXPECT_EQ(6, addr.Address.IPv6[8]);
    EXPECT_EQ(7, addr.Address.IPv6[7]);
    EXPECT_EQ(8, addr.Address.IPv6[6]);
    EXPECT_EQ(9, addr.Address.IPv6[5]);
    EXPECT_EQ(10, addr.Address.IPv6[4]);
    EXPECT_EQ(11, addr.Address.IPv6[3]);
    EXPECT_EQ(12, addr.Address.IPv6[2]);
    EXPECT_EQ(19, addr.Address.IPv6[1]);
    EXPECT_EQ(55, addr.Address.IPv6[0]);
    EXPECT_EQ(AT_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidOctet1_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "300.168.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv6_InvalidOctet1_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "300.128.1.2.3.4.5.6.7.8.9.10.11.12.19.55";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidOctet2_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "200.1680.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv6_InvalidOctet2_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "254.128.1.2.3.4.5.6.7.8.9.10.11.12.19.1055";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_MissingOctet_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "100.168.2";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, NullBuff_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;

    retcode = ExtractPdpAddress(NULL, 0, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, ZeroLen_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "200.1680.2.100";

    retcode = ExtractPdpAddress(addrStr, 0, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_LeadingZero_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "20.0016.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, BuffTooLong_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.2.100";

    retcode = ExtractPdpAddress(addrStr, MAX_IP_STR_LENGTH + 1, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidChar1_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "192.+.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidChar2_Fail)
{
    Retcode_T retcode;
    AT_CGPADDR_Address_T addr;
    const char *addrStr = "192.a.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

FAKE_VALUE_FUNC(Retcode_T, FakeTransceiverWrite, const void *, size_t, size_t *);

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

        xSemaphoreCreateBinaryStatic_fake.return_val = (SemaphoreHandle_t)1;
        xSemaphoreCreateMutexStatic_fake.return_val = (SemaphoreHandle_t)1;
        RingBuffer_Initialize_fake.custom_fake = Fake_RingBuffer_Initialize;
        RingBuffer_Read_fake.custom_fake = Fake_RingBuffer_Read;
        RingBuffer_Peek_fake.custom_fake = FakeRx_RingBuffer_Peek;
        RingBuffer_Write_fake.custom_fake = Fake_RingBuffer_Write;

        Retcode_T rc = AtTransceiver_Initialize(t, reinterpret_cast<uint8_t *>(&transceiverRxBuffer), 0, FakeTransceiverWrite);
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

class TS_At_Set_CREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At_Set_CREG, NIsDisabled_Pass)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Set_CREG(t, AT_CXREG_N_DISABLED);

    ASSERT_EQ(RETCODE_OK, rc);
}

TEST_F(TS_At_Set_CREG, NIsValidValue_Pass)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Set_CREG(t, (AT_CXREG_N_T)random(AT_CXREG_N_DISABLED, AT_CXREG_N_URC_LOC_PSM_CAUSE));

    ASSERT_EQ(RETCODE_OK, rc);
}

class TS_At_Set_CGREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At_Set_CGREG, NIsValidValue_Pass)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Set_CGREG(t, (AT_CXREG_N_T)random(AT_CXREG_N_DISABLED, AT_CXREG_N_URC_LOC_PSM_CAUSE));

    ASSERT_EQ(RETCODE_OK, rc);
}

class TS_At_Set_CEREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At_Set_CEREG, NIsValidValue_Pass)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Set_CEREG(t, (AT_CXREG_N_T)random(AT_CXREG_N_DISABLED, AT_CXREG_N_URC_LOC_PSM_CAUSE));

    ASSERT_EQ(RETCODE_OK, rc);
}

class TS_At_Get_CREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At_Get_CREG, NIsDisabledAndStatIsNotRegistered_Pass)
{
    std::string response = "+CREG: 0,0\r\n\r\nOK\r\n";
    AT_CREG_Param_T param;
    memset(&param, 0, sizeof(param));
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Get_CREG(t, &param);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(AT_CXREG_N_DISABLED, param.N);
    ASSERT_EQ(AT_CXREG_STAT_NOT, param.Stat);
}

TEST_F(TS_At_Get_CREG, NIsDisabledAndStatIsRegisteredHome_Pass)
{
    std::string response = "+CREG: 0,1\r\n\r\nOK\r\n";
    AT_CREG_Param_T param;
    memset(&param, 0, sizeof(param));
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Get_CREG(t, &param);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(AT_CXREG_N_DISABLED, param.N);
    ASSERT_EQ(AT_CXREG_STAT_HOME, param.Stat);
}

TEST_F(TS_At_Get_CREG, NIsURCAndStatIsRegisteredHome_Pass)
{
    std::string response = "+CREG: 1,1\r\n\r\nOK\r\n";
    AT_CREG_Param_T param;
    memset(&param, 0, sizeof(param));
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Get_CREG(t, &param);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(AT_CXREG_N_URC, param.N);
    ASSERT_EQ(AT_CXREG_STAT_HOME, param.Stat);
}

TEST_F(TS_At_Get_CREG, Given_N_is_URC_LOC_and_Stat_NOT_and_invalid_LOC_return_ok_and_valid_response)
{
    std::string response = "+CREG: 2,0,\"FFFF\",\"FFFFFFFF\"\r\n\r\nOK\r\n";
    AT_CREG_Param_T param;
    memset(&param, 0, sizeof(param));
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    Retcode_T rc = At_Get_CREG(t, &param);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(AT_CXREG_N_URC_LOC, param.N);
    ASSERT_EQ(AT_CXREG_STAT_NOT, param.Stat);
    ASSERT_EQ(AT_INVALID_LAC, param.Lac);
    ASSERT_EQ(AT_INVALID_CI, param.Ci);
    ASSERT_EQ(AT_CXREG_ACT_INVALID, param.AcT);
}
