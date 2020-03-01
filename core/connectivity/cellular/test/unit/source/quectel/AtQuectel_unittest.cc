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
 *    Robert Bosch GmbH - implement Quectel BG96 variant
 *
 ******************************************************************************/

#include <gtest.h>
#include <fff.h>

FFF_DEFINITION_BLOCK_START
extern "C"
{
#include "Quectel.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ATQUECTEL
}
FFF_DEFINITION_BLOCK_END

#include "ModemEmulator.cc"

FFF_DEFINITION_BLOCK_START
extern "C"
{
#include "Kiso_Assert_th.hh"
#include "Kiso_Retcode_th.hh"
#include "Kiso_Logging_th.hh"
#include "Kiso_RingBuffer_th.hh"
#include "FreeRTOS_th.hh"
#include "task_th.hh"
#include "semphr_th.hh"

#undef KISO_MODULE_ID
#include "AtQuectel.c"
}
FFF_DEFINITION_BLOCK_END

#include "Kiso_CellularConfig.h"
#ifdef CELLULAR_VARIANT_QUECTEL

class TS_ParseIPv6LeftToRight : public testing::Test
{
protected:
    virtual void SetUp()
    {
        FFF_RESET_HISTORY();
        srand(time(NULL));
    }
};

TEST_F(TS_ParseIPv6LeftToRight, Given__8_hextets__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       expAddr.Address.IPv6[3],
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__7_hextets__4th_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[3] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 "::%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       /* gap */
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__6_hextets__2nd_and_3rd_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 "::%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       /* gap */
                       /* gap */
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__4_hextets__2nd_3rd_4th_5th_and_6th_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[5] = 0;
    expAddr.Address.IPv6[4] = 0;
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 "::%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       /* gap */
                       /* gap */
                       /* gap */
                       /* gap */
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__2_hextets__all_but_1st_and_8th_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[6] = 0;
    expAddr.Address.IPv6[5] = 0;
    expAddr.Address.IPv6[4] = 0;
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    expAddr.Address.IPv6[1] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 "::%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__7_hextets__1st_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[0] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 "::",
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       expAddr.Address.IPv6[3],
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__3_hextets__first_5_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[4] = 0;
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    expAddr.Address.IPv6[1] = 0;
    expAddr.Address.IPv6[0] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 "::",
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__1_hextet__first_7_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[6] = 0;
    expAddr.Address.IPv6[5] = 0;
    expAddr.Address.IPv6[4] = 0;
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    expAddr.Address.IPv6[1] = 0;
    expAddr.Address.IPv6[0] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 "::",
                       expAddr.Address.IPv6[7]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__7_hextets__8th_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[7] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "::%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       expAddr.Address.IPv6[3],
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__5_hextets__6th_7th_and_8th_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[7] = 0;
    expAddr.Address.IPv6[6] = 0;
    expAddr.Address.IPv6[5] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "::%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[4],
                       expAddr.Address.IPv6[3],
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__1_hextet__all_but_1st_skipped__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)rand();
    }
    expAddr.Address.IPv6[7] = 0;
    expAddr.Address.IPv6[6] = 0;
    expAddr.Address.IPv6[5] = 0;
    expAddr.Address.IPv6[4] = 0;
    expAddr.Address.IPv6[3] = 0;
    expAddr.Address.IPv6[2] = 0;
    expAddr.Address.IPv6[1] = 0;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "::%" PRIx16,
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__8_hextets__all_zero__When__parsing_ipv6__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T expAddr;
    for (uint32_t i = 0; i < ATQUECTEL_IPV6GROUPCOUNT; i++)
    {
        expAddr.Address.IPv6[i] = (uint16_t)0;
    }
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                       expAddr.Address.IPv6[7],
                       expAddr.Address.IPv6[6],
                       expAddr.Address.IPv6[5],
                       expAddr.Address.IPv6[4],
                       expAddr.Address.IPv6[3],
                       expAddr.Address.IPv6[2],
                       expAddr.Address.IPv6[1],
                       expAddr.Address.IPv6[0]);
    printf("%.*s\n", len, ipToParse);
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, len, &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv6, addr.Address.IPv6, sizeof(expAddr.Address.IPv6)));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__8_hextets__4th_hextet_too_big__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEF:F0123:4567:89AB:CDEF";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__8_hextets__1st_hextet_too_big__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123F:4567:89AB:CDEF:0123:4567:89AB:CDEF";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__8_hextets__4th_hextet_invalid_hexchar__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEG:0123:4567:89AB:CDEF";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__5_hextets__2nd_to_4th_skipped__1st_hextet_too_big__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEF::0123F";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__6_hextets__3rd_and_5th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB::0123::89AB:CDEF";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__5_hextets__3rd_to_5th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB::::89AB:CDEF";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__6_hextets__1st_and_5th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB::0123:89AB:CDEF::";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__6_hextets__1st_and_8th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "::0123:4567:89AB:0123:89AB:CDEF::";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__9_hextets__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEF:0123:4567:89AB:CDEF:0123";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__9_hextets__5th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEF:0123::4567:89AB:CDEF:0123";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__10_hextets__9th_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "::0123:4567:89AB:CDEF:0123:4567:89AB:CDEF:0123";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv6LeftToRight, Given__10_hextets__1st_skipped__When__parsing_ipv6__Then__return_failure)
{
    Retcode_T retcode = RETCODE_OK;
    const char *ipToParse = "0123:4567:89AB:CDEF:0123:4567:89AB:CDEF:0123::";
    AtQuectel_Address_T addr;
    addr.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    memset(addr.Address.IPv6, 0, sizeof(addr.Address.IPv6));

    retcode = ParseIPv6LeftToRight(ipToParse, (uint32_t)strlen(ipToParse), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

class TS_ParseIPv4 : public testing::Test
{
protected:
    virtual void SetUp()
    {
        FFF_RESET_HISTORY();
        srand(time(NULL));
    }
};

TEST_F(TS_ParseIPv4, Given__valid_random_4_octets__When__parsing_ipv4__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T parsedAddr;
    AtQuectel_Address_T expAddr;
    expAddr.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    for (uint32_t i = 0; i < ATQUECTEL_IPV4GROUPCOUNT; i++)
    {
        expAddr.Address.IPv4[i] = (uint8_t)rand();
    }
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
                       expAddr.Address.IPv4[3],
                       expAddr.Address.IPv4[2],
                       expAddr.Address.IPv4[1],
                       expAddr.Address.IPv4[0]);
    printf("%.*s\n", len, ipToParse);

    retcode = ParseIPv4(ipToParse, len, &parsedAddr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv4, parsedAddr.Address.IPv4, sizeof(expAddr.Address.IPv4)));
}

TEST_F(TS_ParseIPv4, Given__valid_random_4_octets__1st_leading_zero__When__parsing_ipv4__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T parsedAddr;
    AtQuectel_Address_T expAddr;
    expAddr.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    for (uint32_t i = 0; i < ATQUECTEL_IPV4GROUPCOUNT; i++)
    {
        expAddr.Address.IPv4[i] = (uint8_t)rand();
    }
    expAddr.Address.IPv4[3] = 20;
    char ipToParse[ATQUECTEL_MAXIPSTRLENGTH + 1];
    int len = snprintf(ipToParse, sizeof(ipToParse), "%03" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "",
                       expAddr.Address.IPv4[3],
                       expAddr.Address.IPv4[2],
                       expAddr.Address.IPv4[1],
                       expAddr.Address.IPv4[0]);
    printf("%.*s\n", len, ipToParse);

    retcode = ParseIPv4(ipToParse, len, &parsedAddr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(0, memcmp(expAddr.Address.IPv4, parsedAddr.Address.IPv4, sizeof(expAddr.Address.IPv4)));
}

TEST_F(TS_ParseIPv4, Given__4_octets__3rd_numeric_value_too_high__When__parsing_ipv4__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T parsedAddr;
    const char *ipToParse = "123.300.123.123";
    printf("%.*s\n", (int)strlen(ipToParse), ipToParse);

    retcode = ParseIPv4(ipToParse, strlen(ipToParse), &parsedAddr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv4, Given__4_octets__3rd_too_many_digits__When__parsing_ipv4__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T parsedAddr;
    const char *ipToParse = "123.2000.123.123";
    printf("%.*s\n", (int)strlen(ipToParse), ipToParse);

    retcode = ParseIPv4(ipToParse, strlen(ipToParse), &parsedAddr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ParseIPv4, Given__5_octets__When__parsing_ipv4__Then__parse_correctly__return_ok)
{
    Retcode_T retcode = RETCODE_OK;
    AtQuectel_Address_T parsedAddr;
    const char *ipToParse = "123.123.123.123.123";
    printf("%.*s\n", (int)strlen(ipToParse), ipToParse);

    retcode = ParseIPv4(ipToParse, strlen(ipToParse), &parsedAddr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

class TS_AtQuectel_SetQCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQCFG, Given____When__configuring_nwscanmode__Then__valid_message_written__return_ok)
{
    AtQuectel_QCFG_Set_T set;
    set.Setting = ATQUECTEL_QCFG_SETTING_NWSCANMODE;
    set.Value.NwScanMode.ScanMode = ATQUECTEL_QCFG_SCANMODE_AUTOMATIC;
    set.Value.NwScanMode.TakeEffectImmediately = true;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanmode\"," << set.Value.NwScanMode.ScanMode << "," << (int)set.Value.NwScanMode.TakeEffectImmediately << "\r\n";

    Retcode_T rc = AtQuectel_SetQCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQCFG, Given____When__configuring_nwscanseq__Then__valid_message_written__return_ok)
{
    AtQuectel_QCFG_Set_T set;
    strcpy(set.Value.NwScanSeq.ScanSeq, "010203");
    set.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    set.Value.NwScanSeq.TakeEffectImmediately = true;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"," << set.Value.NwScanSeq.ScanSeq << "," << (int)set.Value.NwScanSeq.TakeEffectImmediately << "\r\n";

    Retcode_T rc = AtQuectel_SetQCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQCFG, Given____When__configuring_iotopmode__Then__valid_message_written__return_ok)
{
    AtQuectel_QCFG_Set_T set;
    set.Setting = ATQUECTEL_QCFG_SETTING_IOTOPMODE;
    set.Value.IoTOpMode.Mode = ATQUECTEL_QCFG_IOTOPMODE_LTECATM1;
    set.Value.IoTOpMode.TakeEffectImmediately = true;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"iotopmode\"," << set.Value.IoTOpMode.Mode << "," << (int)set.Value.IoTOpMode.TakeEffectImmediately << "\r\n";

    Retcode_T rc = AtQuectel_SetQCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_QueryQCFG, Given__scanmode_gsmonly__When__reading_nwscanseq_config__Then__valid_message_written__obtain_scanseq__return_ok)
{
    const char *expScanSeq = "01";
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"nwscanseq\"," << expScanSeq << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_STREQ(expScanSeq, resp.Value.NwScanSeq.ScanSeq);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQCFG, Given__scanmode_gsmonly_with_some_whitespace__When__reading_nwscanseq_config__Then__valid_message_written__obtain_scanseq__return_ok)
{
    const char *expScanSeq = "01";
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"nwscanseq\",    " << expScanSeq << "   \r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_STREQ(expScanSeq, resp.Value.NwScanSeq.ScanSeq);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQCFG, Given__scanseq_gsm_ltecatm1_ltecatnb1__When__reading_nwscanseq_config__Then__valid_message_written__obtain_scanseq__return_ok)
{
    const char *expScanSeq = "010203";
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"nwscanseq\"," << expScanSeq << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_STREQ(expScanSeq, resp.Value.NwScanSeq.ScanSeq);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQCFG, Given__scanseq_gsm_ltecatm1__When__reading_nwscanseq_config__Then__valid_message_written__obtain_scanseq__return_ok)
{
    const char *expScanSeq = "0102";
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"nwscanseq\"," << expScanSeq << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_STREQ(expScanSeq, resp.Value.NwScanSeq.ScanSeq);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQCFG, Given__scanseq_ltecatm1__When__reading_nwscanseq_config__Then__valid_message_written__obtain_scanseq__return_ok)
{
    const char *expScanSeq = "02";
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"nwscanseq\"," << expScanSeq << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"nwscanseq\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_STREQ(expScanSeq, resp.Value.NwScanSeq.ScanSeq);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQCFG, Given__iotopmode_ltecatm1__When__reading_iotopmode_config__Then__valid_message_written__obtain_iotopmode__return_ok)
{
    AtQuectel_QCFG_IoTOpMode_Mode_T expIotOpMode = ATQUECTEL_QCFG_IOTOPMODE_LTECATM1;
    AtQuectel_QCFG_Query_T query;
    query.Setting = ATQUECTEL_QCFG_SETTING_IOTOPMODE;
    AtQuectel_QCFG_QueryResponse_T resp;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCFG << ": \"iotopmode\"," << expIotOpMode << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCFG << "="
            << "\"iotopmode\"\r\n";

    Retcode_T rc = AtQuectel_QueryQCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(query.Setting, resp.Setting);
    ASSERT_EQ(expIotOpMode, resp.Value.IoTOpMode.Mode);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQURCCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQURCCFG, Given____When__configuring_urcport_to_usbat__Then__valid_message_written__return_ok)
{
    AtQuectel_QURCCFG_Set_T set;
    set.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    set.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_USBAT;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\",\"" << ARG_USBAT << "\"\r\n";

    Retcode_T rc = AtQuectel_SetQURCCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQURCCFG, Given____When__configuring_urcport_to_usbmodem__Then__valid_message_written__return_ok)
{
    AtQuectel_QURCCFG_Set_T set;
    set.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    set.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_USBMODEM;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\",\"" << ARG_USBMODEM << "\"\r\n";

    Retcode_T rc = AtQuectel_SetQURCCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQURCCFG, Given____When__configuring_urcport_to_uart1__Then__valid_message_written__return_ok)
{
    AtQuectel_QURCCFG_Set_T set;
    set.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    set.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_UART1;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\",\"" << ARG_UART1 << "\"\r\n";

    Retcode_T rc = AtQuectel_SetQURCCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQURCCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_QueryQURCCFG, Given__urc_on_usbat__When__reading_urcport_config__Then__valid_message_written__obtain_urcportvalue__return_ok)
{
    AtQuectel_QURCCFG_Query_T query;
    query.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    AtQuectel_QURCCFG_QueryResponse_T resp;
    resp.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_INVALID;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QURCCFG << ": \"" << ARG_URCPORT << "\",\"" << ARG_USBAT << "\""
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\""
            << "\r\n";

    Retcode_T rc = AtQuectel_QueryQURCCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(ATQUECTEL_QURCCFG_URCPORTVALUE_USBAT, resp.Value.UrcPort.UrcPortValue);
}

TEST_F(TS_AtQuectel_QueryQURCCFG, Given__urc_on_usbmodem__When__reading_urcport_config__Then__valid_message_written__obtain_urcportvalue__return_ok)
{
    AtQuectel_QURCCFG_Query_T query;
    query.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    AtQuectel_QURCCFG_QueryResponse_T resp;
    resp.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_INVALID;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QURCCFG << ": \"" << ARG_URCPORT << "\",\"" << ARG_USBMODEM << "\""
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\""
            << "\r\n";

    Retcode_T rc = AtQuectel_QueryQURCCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(ATQUECTEL_QURCCFG_URCPORTVALUE_USBMODEM, resp.Value.UrcPort.UrcPortValue);
}

TEST_F(TS_AtQuectel_QueryQURCCFG, Given__urc_on_uart1__When__reading_urcport_config__Then__valid_message_written__obtain_urcportvalue__return_ok)
{
    AtQuectel_QURCCFG_Query_T query;
    query.Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    AtQuectel_QURCCFG_QueryResponse_T resp;
    resp.Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_INVALID;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QURCCFG << ": \"" << ARG_URCPORT << "\",\"" << ARG_UART1 << "\""
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QURCCFG << "="
            << "\"" << ARG_URCPORT << "\""
            << "\r\n";

    Retcode_T rc = AtQuectel_QueryQURCCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(ATQUECTEL_QURCCFG_URCPORTVALUE_UART1, resp.Value.UrcPort.UrcPortValue);
}

class TS_AtQuectel_ExecuteQCCID : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_ExecuteQCCID, Given__20_digits_long_iccid__When__polling_iccid__Then__obtain_iccid__return_ok)
{
    AtQuectel_QCCID_ExecuteResponse_T resp;
    memset(resp.Iccid, 0U, sizeof(resp.Iccid));

    char expIccid[ATQUECTEL_QCCID_MAXLENGTH + 1];
    for (size_t i = 0; i < ATQUECTEL_QCCID_MAXLENGTH; ++i)
    {
        expIccid[i] = (char)random('0', '9');
    }
    expIccid[ATQUECTEL_QCCID_MAXLENGTH] = '\0';

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCCID << ": " << expIccid
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCCID << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQCCID(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_STREQ(expIccid, resp.Iccid);
    ASSERT_EQ(20U, strlen(resp.Iccid));
}

TEST_F(TS_AtQuectel_ExecuteQCCID, Given__20_digits_long_iccid_without_whitespace__When__polling_iccid__Then__obtain_iccid__return_ok)
{
    AtQuectel_QCCID_ExecuteResponse_T resp;
    memset(resp.Iccid, 0U, sizeof(resp.Iccid));

    char expIccid[ATQUECTEL_QCCID_MAXLENGTH + 1];
    for (size_t i = 0; i < ATQUECTEL_QCCID_MAXLENGTH; ++i)
    {
        expIccid[i] = (char)random('0', '9');
    }
    expIccid[ATQUECTEL_QCCID_MAXLENGTH] = '\0';

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCCID << ":" << expIccid
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCCID << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQCCID(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_STREQ(expIccid, resp.Iccid);
    ASSERT_EQ(20U, strlen(resp.Iccid));
}

TEST_F(TS_AtQuectel_ExecuteQCCID, Given__19_digits_long_iccid__When__polling_iccid__Then__obtain_iccid__return_ok)
{
    AtQuectel_QCCID_ExecuteResponse_T resp;
    memset(resp.Iccid, 0U, sizeof(resp.Iccid));

    char expIccid[ATQUECTEL_QCCID_MAXLENGTH];
    for (size_t i = 0; i < ATQUECTEL_QCCID_MAXLENGTH - 1; ++i)
    {
        expIccid[i] = (char)random('0', '9');
    }
    expIccid[ATQUECTEL_QCCID_MAXLENGTH - 1] = '\0';

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCCID << ": " << expIccid
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCCID << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQCCID(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_STREQ(expIccid, resp.Iccid);
    ASSERT_EQ(19U, strlen(resp.Iccid));
}

TEST_F(TS_AtQuectel_ExecuteQCCID, Given__19_digits_long_iccid_without_whitespace__When__polling_iccid__Then__obtain_iccid__return_ok)
{
    AtQuectel_QCCID_ExecuteResponse_T resp;
    memset(resp.Iccid, 0U, sizeof(resp.Iccid));

    char expIccid[ATQUECTEL_QCCID_MAXLENGTH];
    for (size_t i = 0; i < ATQUECTEL_QCCID_MAXLENGTH - 1; ++i)
    {
        expIccid[i] = (char)random('0', '9');
    }
    expIccid[ATQUECTEL_QCCID_MAXLENGTH - 1] = '\0';

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QCCID << ":" << expIccid
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QCCID << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQCCID(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_STREQ(expIccid, resp.Iccid);
    ASSERT_EQ(19U, strlen(resp.Iccid));
}

class TS_AtQuectel_SetQINDCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__enabling_indication_of_all_urcs__not_saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_ALL;
    set.Enable = true;
    set.SaveToNonVolatileRam = false;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_ALL << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__disabling_indication_of_all_urcs__saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_ALL;
    set.Enable = false;
    set.SaveToNonVolatileRam = true;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_ALL << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__enabling_indication_of_csq_urcs__not_saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_CSQ;
    set.Enable = true;
    set.SaveToNonVolatileRam = false;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_CSQ << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__enabling_indication_of_smsfull_urcs__not_saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_SMSFULL;
    set.Enable = true;
    set.SaveToNonVolatileRam = false;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_SMSFULL << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__enabling_indication_of_ring_urcs__not_saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_RING;
    set.Enable = true;
    set.SaveToNonVolatileRam = false;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_RING << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQINDCFG, Given____When__enabling_indication_of_smsincoming_urcs__not_saving_to_nvram__Then__valid_message_written__return_ok)
{
    AtQuectel_QINDCFG_Set_T set;
    set.UrcType = ATQUECTEL_QINDCFG_URCTYPE_SMSINCOMING;
    set.Enable = true;
    set.SaveToNonVolatileRam = false;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_SMSINCOMING << "\"," << set.Enable << "," << set.SaveToNonVolatileRam << "\r\n";

    Retcode_T rc = AtQuectel_SetQINDCFG(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQINDCFG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_QueryQINDCFG, Given__indication_of_all_urcs_on__When__reading_urc_config__Then__obtain_urc_config__return_ok)
{
    bool expEnable = true;
    AtQuectel_QINDCFG_Query_T query;
    query.UrcType = ATQUECTEL_QINDCFG_URCTYPE_ALL;
    AtQuectel_QINDCFG_QueryResponse_T resp;
    resp.Enable = !expEnable;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QINDCFG << ": \"" << ARG_ALL << "\", " << expEnable
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_ALL << "\"\r\n";

    Retcode_T rc = AtQuectel_QueryQINDCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expEnable, resp.Enable);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQINDCFG, Given__indication_of_all_urcs_off__When__reading_urc_config__Then__obtain_urc_config__return_ok)
{
    bool expEnable = false;
    AtQuectel_QINDCFG_Query_T query;
    query.UrcType = ATQUECTEL_QINDCFG_URCTYPE_ALL;
    AtQuectel_QINDCFG_QueryResponse_T resp;
    resp.Enable = !expEnable;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QINDCFG << ": \"" << ARG_ALL << "\", " << expEnable
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_ALL << "\"\r\n";

    Retcode_T rc = AtQuectel_QueryQINDCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expEnable, resp.Enable);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQINDCFG, Given__indication_of_csq_urcs_on__When__reading_urc_config__Then__obtain_urc_config__return_ok)
{
    bool expEnable = true;
    AtQuectel_QINDCFG_Query_T query;
    query.UrcType = ATQUECTEL_QINDCFG_URCTYPE_CSQ;
    AtQuectel_QINDCFG_QueryResponse_T resp;
    resp.Enable = !expEnable;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QINDCFG << ": \"" << ARG_ALL << "\", " << expEnable
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINDCFG << "=\"" << ARG_CSQ << "\"\r\n";

    Retcode_T rc = AtQuectel_QueryQINDCFG(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expEnable, resp.Enable);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_ExecuteQINISTAT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_ExecuteQINISTAT, Given__initial_state__When__polling_sim_init_state__Then__obtain_status__return_ok)
{
    AtQuectel_QINISTAT_Status_T expStates = (AtQuectel_QINISTAT_Status_T)(ATQUECTEL_QINISTAT_STATUS_INITIALSTATE);
    AtQuectel_QINISTAT_ExecuteResponse_T resp;
    resp.Status = (AtQuectel_QINISTAT_Status_T)!ATQUECTEL_QINISTAT_STATUS_INITIALSTATE;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QINISTAT << ": " << expStates << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINISTAT << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQINISTAT(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expStates, resp.Status);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_ExecuteQINISTAT, Given__cpin_ready__sms_init_complete__When__polling_sim_init_state__Then__obtain_status__return_ok)
{
    AtQuectel_QINISTAT_Status_T expStates = (AtQuectel_QINISTAT_Status_T)(ATQUECTEL_QINISTAT_STATUS_INITIALSTATE | ATQUECTEL_QINISTAT_STATUS_CPINREADY | ATQUECTEL_QINISTAT_STATUS_SMSINITCOMPLETE);
    AtQuectel_QINISTAT_ExecuteResponse_T resp;
    resp.Status = ATQUECTEL_QINISTAT_STATUS_INITIALSTATE;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QINISTAT << ": " << expStates << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QINISTAT << "\r\n";

    Retcode_T rc = AtQuectel_ExecuteQINISTAT(this->t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expStates, resp.Status);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQICSGP : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_QueryQICSGP, Given__unconfigured_context__When__reading_parameters_of_context_1__Then__valid_message_written__obtain_result__return_ok)
{
    AtQUectel_QICSGP_ContextType_T expContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_NOTCONFIGURED;
    const char *expApn = "";
    const char *expUsername = "";
    const char *expPassword = "";
    AtQuectel_QICSGP_Authentication_T expAuth = ATQUECTEL_QICSGP_AUTHENTICATION_NONE;
    char apnBuffer[32];
    char usernameBuffer[32];
    char passwordBuffer[32];
    AtQuectel_QICSGP_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(AtQuectel_QICSGP_QueryResponse_T));
    resp.Apn = apnBuffer;
    resp.ApnSize = sizeof(apnBuffer);
    resp.Username = usernameBuffer;
    resp.UsernameSize = sizeof(usernameBuffer);
    resp.Password = passwordBuffer;
    resp.PasswordSize = sizeof(passwordBuffer);

    AtQuectel_QICSGP_Query_T query;
    query.ContextId = 1;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QICSGP << ": " << expContextType << ",\""
             << expApn << "\",\"" << expUsername << "\",\"" << expPassword << "\","
             << expAuth << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << query.ContextId << "\r\n";

    Retcode_T rc = AtQuectel_QueryQICSGP(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expContextType, resp.ContextType);
    ASSERT_STREQ(expApn, resp.Apn);
    ASSERT_STREQ(expUsername, resp.Username);
    ASSERT_STREQ(expPassword, resp.Password);
    ASSERT_EQ(expAuth, resp.Authentication);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQICSGP, Given__ipv4_context__no_auth__When__reading_parameters_of_context_2__Then__valid_message_written__obtain_result__return_ok)
{
    AtQUectel_QICSGP_ContextType_T expContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_IPV4;
    const char *expApn = "this.is.a.test";
    const char *expUsername = "";
    const char *expPassword = "";
    AtQuectel_QICSGP_Authentication_T expAuth = ATQUECTEL_QICSGP_AUTHENTICATION_NONE;
    char apnBuffer[strlen(expApn) + 1];
    char usernameBuffer[strlen(expUsername) + 1];
    char passwordBuffer[strlen(expPassword) + 1];
    AtQuectel_QICSGP_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(AtQuectel_QICSGP_QueryResponse_T));
    resp.Apn = apnBuffer;
    resp.ApnSize = sizeof(apnBuffer);
    resp.Username = usernameBuffer;
    resp.UsernameSize = sizeof(usernameBuffer);
    resp.Password = passwordBuffer;
    resp.PasswordSize = sizeof(passwordBuffer);

    AtQuectel_QICSGP_Query_T query;
    query.ContextId = 2;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QICSGP << ": " << expContextType << ",\""
             << expApn << "\",\"" << expUsername << "\",\"" << expPassword << "\","
             << expAuth << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << query.ContextId << "\r\n";

    Retcode_T rc = AtQuectel_QueryQICSGP(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expContextType, resp.ContextType);
    ASSERT_STREQ(expApn, resp.Apn);
    ASSERT_STREQ(expUsername, resp.Username);
    ASSERT_STREQ(expPassword, resp.Password);
    ASSERT_EQ(expAuth, resp.Authentication);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQICSGP, Given__ipv6_context__chap_auth__When__reading_parameters_of_context_1__Then__valid_message_written__obtain_result__return_ok)
{
    AtQUectel_QICSGP_ContextType_T expContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_IPV6;
    const char *expApn = "this.is.a.test";
    const char *expUsername = "user";
    const char *expPassword = "password";
    AtQuectel_QICSGP_Authentication_T expAuth = ATQUECTEL_QICSGP_AUTHENTICATION_CHAP;
    char apnBuffer[strlen(expApn) + 1];
    char usernameBuffer[strlen(expUsername) + 1];
    char passwordBuffer[strlen(expPassword) + 1];
    AtQuectel_QICSGP_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(AtQuectel_QICSGP_QueryResponse_T));
    resp.Apn = apnBuffer;
    resp.ApnSize = sizeof(apnBuffer);
    resp.Username = usernameBuffer;
    resp.UsernameSize = sizeof(usernameBuffer);
    resp.Password = passwordBuffer;
    resp.PasswordSize = sizeof(passwordBuffer);

    AtQuectel_QICSGP_Query_T query;
    query.ContextId = 1;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QICSGP << ": " << expContextType << ",\""
             << expApn << "\",\"" << expUsername << "\",\"" << expPassword << "\","
             << expAuth << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << query.ContextId << "\r\n";

    Retcode_T rc = AtQuectel_QueryQICSGP(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expContextType, resp.ContextType);
    ASSERT_STREQ(expApn, resp.Apn);
    ASSERT_STREQ(expUsername, resp.Username);
    ASSERT_STREQ(expPassword, resp.Password);
    ASSERT_EQ(expAuth, resp.Authentication);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQICSGP : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQICSGP, Given____When_setting_context_1__type_ipv4__some_apn__no_auth__Then__valid_message_written__return_ok)
{
    AtQuectel_QICSGP_Set_T set;
    set.ContextId = 1;
    set.ContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_IPV4;
    set.Apn = "this.is.a.test";
    set.Username = "should be ignored";
    set.Password = "should be ignored";
    set.Authentication = ATQUECTEL_QICSGP_AUTHENTICATION_NONE;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << set.ContextId << ","
            << set.ContextType << ",\"" << set.Apn << "\""
            << "\r\n";

    Retcode_T rc = AtQuectel_SetQICSGP(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQICSGP, Given____When_setting_context_1__type_ipv4__some_apn__pap_auth__Then__valid_message_written__return_ok)
{
    AtQuectel_QICSGP_Set_T set;
    set.ContextId = 1;
    set.ContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_IPV4;
    set.Apn = "this.is.a.test";
    set.Username = "username";
    set.Password = "password";
    set.Authentication = ATQUECTEL_QICSGP_AUTHENTICATION_PAP;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << set.ContextId << ","
            << set.ContextType << ",\"" << set.Apn << "\",\""
            << set.Username << "\",\"" << set.Password << "\","
            << set.Authentication << "\r\n";

    Retcode_T rc = AtQuectel_SetQICSGP(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQICSGP, Given____When_setting_context_3__type_ipv6__some_apn__no_auth__Then__valid_message_written__return_ok)
{
    AtQuectel_QICSGP_Set_T set;
    set.ContextId = 3;
    set.ContextType = ATQUECTEL_QICSGP_CONTEXTTYPE_IPV6;
    set.Apn = "this.is.a.test";
    set.Username = "should be ignored";
    set.Password = "should be ignored";
    set.Authentication = ATQUECTEL_QICSGP_AUTHENTICATION_NONE;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICSGP << "=" << set.ContextId << ","
            << set.ContextType << ",\"" << set.Apn << "\""
            << "\r\n";

    Retcode_T rc = AtQuectel_SetQICSGP(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQIACT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQIACT, Given____When__activating_context_1__Then__valid_message_written__return_ok)
{
    AtQuectel_QIACT_Set_T set;
    set.ContextId = 1;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIACT << "=" << set.ContextId << "\r\n";

    Retcode_T rc = AtQuectel_SetQIACT(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_GetQIACT : public TS_ReadableAndWritableTransceiver
{
protected:
    void Serialize(std::stringstream &os, const AtQuectel_Address_T &addr)
    {
        std::ios init(NULL);
        init.copyfmt(os);
        switch (addr.Type)
        {
        case ATQUECTEL_ADDRESSTYPE_IPV4:
            os << "\"" << std::dec
               << (int)addr.Address.IPv4[3] << "."
               << (int)addr.Address.IPv4[2] << "."
               << (int)addr.Address.IPv4[1] << "."
               << (int)addr.Address.IPv4[0] << "\"";
            break;
        case ATQUECTEL_ADDRESSTYPE_IPV6:
            os << "\"" << std::hex
               << addr.Address.IPv6[7] << ":"
               << addr.Address.IPv6[6] << ":"
               << addr.Address.IPv6[5] << ":"
               << addr.Address.IPv6[4] << ":"
               << addr.Address.IPv6[3] << ":"
               << addr.Address.IPv6[2] << ":"
               << addr.Address.IPv6[1] << ":"
               << addr.Address.IPv6[0] << "\"";
            break;
        default:
            std::cerr << "Invalid addr.Type" << std::endl;
            exit(1);
            break;
        }
        os.copyfmt(init);
    }
};

TEST_F(TS_AtQuectel_GetQIACT, Given__context_1_active_ipv4__When__Then__valid_message_written__context_parameters_obtained__return_ok)
{
    size_t expRespLength = 1;
    AtQuectel_QIACT_GetResponse_T expRespArray[expRespLength];
    expRespArray[0].ContextId = 1;
    expRespArray[0].ContextState = true;
    expRespArray[0].ContextType = ATQUECTEL_QIACT_CONTEXTTYPE_IPV4;
    expRespArray[0].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    expRespArray[0].IpAddress.Address.IPv4[3] = 123;
    expRespArray[0].IpAddress.Address.IPv4[2] = 10;
    expRespArray[0].IpAddress.Address.IPv4[1] = 20;
    expRespArray[0].IpAddress.Address.IPv4[0] = 30;

    size_t respSize = expRespLength + 1;
    AtQuectel_QIACT_GetResponse_T respArray[respSize];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;

    std::stringstream response;
    for (size_t i = 0; i < expRespLength; ++i)
    {
        response << CMD_SEPARATOR << CMD_QIACT << ": "
                 << expRespArray[i].ContextId << ", "
                 << expRespArray[i].ContextState << ", "
                 << expRespArray[i].ContextType << ", ";
        Serialize(response, expRespArray[i].IpAddress);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIACT << "?\r\n";

    Retcode_T rc = AtQuectel_GetQIACT(this->t, respArray, respSize, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ContextState, respArray[i].ContextState);
        ASSERT_EQ(expRespArray[i].ContextType, respArray[i].ContextType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        switch (respArray[i].ContextType)
        {
        case ATQUECTEL_QIACT_CONTEXTTYPE_IPV4:
            ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv4, respArray[i].IpAddress.Address.IPv4, sizeof(AtQuectel_Address_T::Address.IPv4)));
            break;
        case ATQUECTEL_QIACT_CONTEXTTYPE_IPV6:
            ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv6, respArray[i].IpAddress.Address.IPv6, sizeof(AtQuectel_Address_T::Address.IPv6)));
            break;
        default:
            FAIL();
            break;
        }
    }
}

TEST_F(TS_AtQuectel_GetQIACT, Given__context_1_active_ipv4__conext_3_active_ipv6__When__Then__valid_message_written__context_parameters_obtained__return_ok)
{
    size_t expRespLength = 2;
    AtQuectel_QIACT_GetResponse_T expRespArray[expRespLength];
    expRespArray[0].ContextId = 1;
    expRespArray[0].ContextState = true;
    expRespArray[0].ContextType = ATQUECTEL_QIACT_CONTEXTTYPE_IPV4;
    expRespArray[0].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    expRespArray[0].IpAddress.Address.IPv4[3] = 123;
    expRespArray[0].IpAddress.Address.IPv4[2] = 10;
    expRespArray[0].IpAddress.Address.IPv4[1] = 20;
    expRespArray[0].IpAddress.Address.IPv4[0] = 30;

    expRespArray[1].ContextId = 3;
    expRespArray[1].ContextState = true;
    expRespArray[1].ContextType = ATQUECTEL_QIACT_CONTEXTTYPE_IPV6;
    expRespArray[1].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    expRespArray[1].IpAddress.Address.IPv6[7] = 0xFE00;
    expRespArray[1].IpAddress.Address.IPv6[6] = 0xABCD;
    expRespArray[1].IpAddress.Address.IPv6[5] = 0xDCBA;
    expRespArray[1].IpAddress.Address.IPv6[4] = 0xDEAD;
    expRespArray[1].IpAddress.Address.IPv6[3] = 0xBEEF;
    expRespArray[1].IpAddress.Address.IPv6[2] = 0x0123;
    expRespArray[1].IpAddress.Address.IPv6[1] = 0x4567;
    expRespArray[1].IpAddress.Address.IPv6[0] = 0x89AB;

    size_t respSize = expRespLength + 1;
    AtQuectel_QIACT_GetResponse_T respArray[respSize];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;

    std::stringstream response;
    for (size_t i = 0; i < expRespLength; ++i)
    {
        response << CMD_SEPARATOR << CMD_QIACT << ": "
                 << expRespArray[i].ContextId << ", "
                 << expRespArray[i].ContextState << ", "
                 << expRespArray[i].ContextType << ", ";
        Serialize(response, expRespArray[i].IpAddress);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIACT << "?\r\n";

    Retcode_T rc = AtQuectel_GetQIACT(this->t, respArray, respSize, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ContextState, respArray[i].ContextState);
        ASSERT_EQ(expRespArray[i].ContextType, respArray[i].ContextType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        switch (respArray[i].ContextType)
        {
        case ATQUECTEL_QIACT_CONTEXTTYPE_IPV4:
            ASSERT_TRUE(0 == memcmp(expRespArray[i].IpAddress.Address.IPv4, respArray[i].IpAddress.Address.IPv4, 4));
            break;
        case ATQUECTEL_QIACT_CONTEXTTYPE_IPV6:
            ASSERT_TRUE(0 == memcmp(expRespArray[i].IpAddress.Address.IPv6, respArray[i].IpAddress.Address.IPv6, 8));
            break;
        default:
            FAIL();
            break;
        }
    }
}

class TS_AtQuectel_SetQIDEACT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQIDEACT, Given____When__deactivating_context_1__Then__valid_message_written__return_ok)
{
    AtQuectel_QIDEACT_Set_T set;
    set.ContextId = 1;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIDEACT << "=" << set.ContextId << "\r\n";

    Retcode_T rc = AtQuectel_SetQIDEACT(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQIOPEN : public TS_AtQuectel_GetQIACT
{
};

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_0_on_context_1_bound_to_ipv4_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_0_on_context_1_bound_to_ipv6_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[7] = 0xFEFE;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[6] = 0xEFEF;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[5] = 0xABAB;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[4] = 0xBABA;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[3] = 0x0101;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[2] = 0xAAAA;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[1] = 0xBBBB;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv6[0] = 0x1234;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_0_on_context_1_bound_to_domain_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_DOMAINNAME;
    set.RemoteEndpoint.Endpoint.DomainName = "eclipse.org";
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    expSent << "\"" << set.RemoteEndpoint.Endpoint.DomainName << "\"";
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_4_on_context_2_bound_to_ipv4_in_direct_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 2;
    set.ConnectId = 4;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_tcp_socket_0_on_context_1_bound_to_ipv4_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_TCP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_TCP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_tcplistener_socket_0_on_context_1_bound_to_ipv4_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_TCPLISTENER;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_TCPLISTENER << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udpservice_socket_0_on_context_1_bound_to_ipv4_in_buffer_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDPSERVICE << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_0_on_context_1_bound_to_ipv4_in_direct_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_DIRECT;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQIOPEN, Given____When__creating_udp_socket_0_on_context_1_bound_to_ipv4_in_transparent_mode__Then__valid_message_written__return_ok)
{
    AtQuectel_QIOPEN_Set_T set;
    set.ContextId = 1;
    set.ConnectId = 0;
    set.ServiceType = ATQUECTEL_QIOPEN_SERVICETYPE_UDP;
    set.RemoteEndpoint.Type = ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS;
    set.RemoteEndpoint.Endpoint.IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[3] = 123;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[2] = 100;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[1] = 90;
    set.RemoteEndpoint.Endpoint.IpAddress.Address.IPv4[0] = 80;
    set.RemotePort = 1234;
    set.LocalPort = 12345;
    set.AccessMode = ATQUECTEL_DATAACCESSMODE_TRANSPARENT;

    std::stringstream response;
    response << "\r\nCONNECT\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIOPEN << "=" << set.ContextId << "," << set.ConnectId << ",\"" << ARG_UDP << "\",";
    Serialize(expSent, set.RemoteEndpoint.Endpoint.IpAddress);
    expSent << "," << set.RemotePort << "," << set.LocalPort << "," << set.AccessMode << "\r\n";

    Retcode_T rc = AtQuectel_SetQIOPEN(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQICLOSE : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_SetQICLOSE, Given____When__closing_socket_0_with_no_timeout__Then__valid_message_written__return_ok)
{
    AtQuectel_QICLOSE_Set_T set;
    set.ConnectId = 0;
    set.Timeout = 0;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICLOSE << "=" << set.ConnectId << "," << set.Timeout << "\r\n";

    Retcode_T rc = AtQuectel_SetQICLOSE(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQICLOSE, Given____When__closing_socket_2_with_10s_timeout__Then__valid_message_written__return_ok)
{
    AtQuectel_QICLOSE_Set_T set;
    set.ConnectId = 2;
    set.Timeout = 10;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICLOSE << "=" << set.ConnectId << "," << set.Timeout << "\r\n";

    Retcode_T rc = AtQuectel_SetQICLOSE(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQICLOSE, Given____When__closing_socket_1_with_max_timeout__Then__valid_message_written__return_ok)
{
    AtQuectel_QICLOSE_Set_T set;
    set.ConnectId = 1;
    set.Timeout = UINT16_MAX;

    std::stringstream response;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QICLOSE << "=" << set.ConnectId << "," << set.Timeout << "\r\n";

    Retcode_T rc = AtQuectel_SetQICLOSE(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_GetQISTATE : public TS_AtQuectel_GetQIACT
{
protected:
    void Serialize(std::stringstream &os, const AtQuectel_QISTATE_GetResponse_T &resp)
    {
        std::stringstream ss;

        os << resp.ConnectId << ",";
        Serialize(os, resp.ServiceType);
        os << ",";
        TS_AtQuectel_GetQIACT::Serialize(os, resp.IpAddress);
        os << "," << resp.RemotePort << "," << resp.LocalPort << ",";
        os << resp.SocketState << "," << resp.ContextId << ",";
        os << resp.ServerId << "," << resp.AccessMode << ",";
        Serialize(os, resp.AtPort);
    }

    void Serialize(std::stringstream &os, const AtQuectel_QISTATE_ServiceType_T &serviceType)
    {
        switch (serviceType)
        {
        case ATQUECTEL_QISTATE_SERVICETYPE_TCP:
            os << '"' << ARG_TCP << '"';
            break;
        case ATQUECTEL_QISTATE_SERVICETYPE_UDP:
            os << '"' << ARG_UDP << '"';
            break;
        case ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER:
            os << '"' << ARG_TCPLISTENER << '"';
            break;
        case ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING:
            os << '"' << ARG_TCPINCOMING << '"';
            break;
        case ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE:
            os << '"' << ARG_UDPSERVICE << '"';
            break;
        default:
            std::cerr << "Invalid serviceType" << std::endl;
            exit(1);
            break;
        }
    }

    void Serialize(std::stringstream &os, const AtQuectel_QISTATE_AtPort_T &atPort)
    {
        switch (atPort)
        {
        case ATQUECTEL_QISTATE_ATPORT_USBAT:
            os << '"' << ARG_USBAT << '"';
            break;
        case ATQUECTEL_QISTATE_ATPORT_USBMODEM:
            os << '"' << ARG_USBMODEM << '"';
            break;
        case ATQUECTEL_QISTATE_ATPORT_UART1:
            os << '"' << ARG_UART1 << '"';
            break;
        case ATQUECTEL_QISTATE_ATPORT_INVALID:
        default:
            std::cerr << "Invalid atPort" << std::endl;
            exit(1);
            break;
        }
    }
};

TEST_F(TS_AtQuectel_GetQISTATE, Given__single_socket__tcp__ipv4__When__getting_socket_states__Then__valid_message_written__obtain_responses__return_ok)
{
    size_t expRespLength = 1;
    AtQuectel_QISTATE_GetResponse_T respArray[expRespLength];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;
    AtQuectel_QISTATE_GetResponse_T expRespArray[expRespLength];
    for (size_t i = 0; i < expRespLength; ++i)
    {
        expRespArray[i].ConnectId = i;
        expRespArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCP;
        expRespArray[i].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
        expRespArray[i].IpAddress.Address.IPv4[3] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[2] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[1] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[0] = random(0, UINT8_MAX);
        expRespArray[i].RemotePort = random(0, UINT16_MAX);
        expRespArray[i].LocalPort = random(0, UINT16_MAX);
        expRespArray[i].SocketState = ATQUECTEL_QISTATE_SOCKETSTATE_CONNECTED;
        expRespArray[i].ContextId = random(1, 16);
        expRespArray[i].ServerId = 0;
        expRespArray[i].AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;
        expRespArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_UART1;
    }

    std::stringstream response;
    response << "+QISTATE: ";
    for (size_t i = 0; i < expRespLength; ++i)
    {
        Serialize(response, expRespArray[i]);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISTATE << "?\r\n";

    Retcode_T rc = AtQuectel_GetQISTATE(this->t, respArray, expRespLength, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ConnectId, respArray[i].ConnectId);
        ASSERT_EQ(expRespArray[i].ServiceType, respArray[i].ServiceType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv4,
                            respArray[i].IpAddress.Address.IPv4,
                            (expRespArray[i].IpAddress.Type == ATQUECTEL_ADDRESSTYPE_IPV4)
                                ? sizeof(AtQuectel_Address_T::Address.IPv4)
                                : sizeof(AtQuectel_Address_T::Address.IPv6)));
        ASSERT_EQ(expRespArray[i].RemotePort, respArray[i].RemotePort);
        ASSERT_EQ(expRespArray[i].LocalPort, respArray[i].LocalPort);
        ASSERT_EQ(expRespArray[i].SocketState, respArray[i].SocketState);
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ServerId, respArray[i].ServerId);
        ASSERT_EQ(expRespArray[i].AccessMode, respArray[i].AccessMode);
        ASSERT_EQ(expRespArray[i].AtPort, respArray[i].AtPort);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_GetQISTATE, Given__3_sockets__tcplistener_udp_tcpincoming__ipv6__When__getting_socket_states__Then__valid_message_written__obtain_responses__return_ok)
{
    size_t expRespLength = 3;
    AtQuectel_QISTATE_GetResponse_T respArray[expRespLength];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;
    AtQuectel_QISTATE_GetResponse_T expRespArray[expRespLength];
    for (size_t i = 0; i < expRespLength; ++i)
    {
        expRespArray[i].ConnectId = i;
        expRespArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCP;
        expRespArray[i].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
        expRespArray[i].IpAddress.Address.IPv6[7] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[6] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[5] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[4] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[3] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[2] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[1] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[0] = random(0, UINT16_MAX);
        expRespArray[i].RemotePort = random(0, UINT16_MAX);
        expRespArray[i].LocalPort = random(0, UINT16_MAX);
        expRespArray[i].SocketState = ATQUECTEL_QISTATE_SOCKETSTATE_CONNECTED;
        expRespArray[i].ContextId = random(1, 16);
        expRespArray[i].ServerId = 0;
        expRespArray[i].AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;
        expRespArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_UART1;
    }
    expRespArray[0].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER;
    expRespArray[1].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_UDP;
    expRespArray[2].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING;

    std::stringstream response;
    for (size_t i = 0; i < expRespLength; ++i)
    {
        response << "+QISTATE: ";
        Serialize(response, expRespArray[i]);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISTATE << "?\r\n";

    Retcode_T rc = AtQuectel_GetQISTATE(this->t, respArray, expRespLength, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ConnectId, respArray[i].ConnectId);
        ASSERT_EQ(expRespArray[i].ServiceType, respArray[i].ServiceType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv4,
                            respArray[i].IpAddress.Address.IPv4,
                            (expRespArray[i].IpAddress.Type == ATQUECTEL_ADDRESSTYPE_IPV4)
                                ? sizeof(AtQuectel_Address_T::Address.IPv4)
                                : sizeof(AtQuectel_Address_T::Address.IPv6)));
        ASSERT_EQ(expRespArray[i].RemotePort, respArray[i].RemotePort);
        ASSERT_EQ(expRespArray[i].LocalPort, respArray[i].LocalPort);
        ASSERT_EQ(expRespArray[i].SocketState, respArray[i].SocketState);
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ServerId, respArray[i].ServerId);
        ASSERT_EQ(expRespArray[i].AccessMode, respArray[i].AccessMode);
        ASSERT_EQ(expRespArray[i].AtPort, respArray[i].AtPort);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQISTATE : public TS_AtQuectel_GetQISTATE
{
};

TEST_F(TS_AtQuectel_QueryQISTATE, Given__single_socket__tcp__ipv4__When__querying_state_of_socket_1__Then__valid_message_written__obtain_responses__return_ok)
{
    AtQuectel_QISTATE_Query_T query;
    query.QueryType = ATQUECTEL_QISTATE_QUERYTYPE_CONNECTID;
    query.Query.ConnectId = 1;

    size_t expRespLength = 1;
    AtQuectel_QISTATE_GetResponse_T respArray[expRespLength];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;
    AtQuectel_QISTATE_GetResponse_T expRespArray[expRespLength];
    for (size_t i = 0; i < expRespLength; ++i)
    {
        expRespArray[i].ConnectId = query.Query.ConnectId;
        expRespArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCP;
        expRespArray[i].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
        expRespArray[i].IpAddress.Address.IPv4[3] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[2] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[1] = random(0, UINT8_MAX);
        expRespArray[i].IpAddress.Address.IPv4[0] = random(0, UINT8_MAX);
        expRespArray[i].RemotePort = random(0, UINT16_MAX);
        expRespArray[i].LocalPort = random(0, UINT16_MAX);
        expRespArray[i].SocketState = ATQUECTEL_QISTATE_SOCKETSTATE_CONNECTED;
        expRespArray[i].ContextId = random(1, 16);
        expRespArray[i].ServerId = 0;
        expRespArray[i].AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;
        expRespArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_UART1;
    }

    std::stringstream response;
    response << "+QISTATE: ";
    for (size_t i = 0; i < expRespLength; ++i)
    {
        Serialize(response, expRespArray[i]);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISTATE << "=" << query.QueryType
            << "," << query.Query.ConnectId << "\r\n";

    Retcode_T rc = AtQuectel_QueryQISTATE(this->t, &query, respArray, expRespLength, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ConnectId, respArray[i].ConnectId);
        ASSERT_EQ(expRespArray[i].ServiceType, respArray[i].ServiceType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv4,
                            respArray[i].IpAddress.Address.IPv4,
                            (expRespArray[i].IpAddress.Type == ATQUECTEL_ADDRESSTYPE_IPV4)
                                ? sizeof(AtQuectel_Address_T::Address.IPv4)
                                : sizeof(AtQuectel_Address_T::Address.IPv6)));
        ASSERT_EQ(expRespArray[i].RemotePort, respArray[i].RemotePort);
        ASSERT_EQ(expRespArray[i].LocalPort, respArray[i].LocalPort);
        ASSERT_EQ(expRespArray[i].SocketState, respArray[i].SocketState);
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ServerId, respArray[i].ServerId);
        ASSERT_EQ(expRespArray[i].AccessMode, respArray[i].AccessMode);
        ASSERT_EQ(expRespArray[i].AtPort, respArray[i].AtPort);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_QueryQISTATE, Given__3_sockets__tcplistener_udp_tcpincoming__ipv6__When__querying_state_of_sockets_on_context_1__Then__valid_message_written__obtain_responses__return_ok)
{
    AtQuectel_QISTATE_Query_T query;
    query.QueryType = ATQUECTEL_QISTATE_QUERYTYPE_CONTEXTID;
    query.Query.ContextId = 1;

    size_t expRespLength = 3;
    AtQuectel_QISTATE_GetResponse_T respArray[expRespLength];
    memset(respArray, 0xAA, sizeof(respArray));
    size_t respLength = 0;
    AtQuectel_QISTATE_GetResponse_T expRespArray[expRespLength];
    for (size_t i = 0; i < expRespLength; ++i)
    {
        expRespArray[i].ConnectId = i;
        expRespArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCP;
        expRespArray[i].IpAddress.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
        expRespArray[i].IpAddress.Address.IPv6[7] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[6] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[5] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[4] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[3] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[2] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[1] = random(0, UINT16_MAX);
        expRespArray[i].IpAddress.Address.IPv6[0] = random(0, UINT16_MAX);
        expRespArray[i].RemotePort = random(0, UINT16_MAX);
        expRespArray[i].LocalPort = random(0, UINT16_MAX);
        expRespArray[i].SocketState = ATQUECTEL_QISTATE_SOCKETSTATE_CONNECTED;
        expRespArray[i].ContextId = query.Query.ContextId;
        expRespArray[i].ServerId = 0;
        expRespArray[i].AccessMode = ATQUECTEL_DATAACCESSMODE_BUFFER;
        expRespArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_UART1;
    }
    expRespArray[0].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER;
    expRespArray[1].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_UDP;
    expRespArray[2].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING;

    std::stringstream response;
    for (size_t i = 0; i < expRespLength; ++i)
    {
        response << "+QISTATE: ";
        Serialize(response, expRespArray[i]);
        response << "\r\n";
    }
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISTATE << "=" << query.QueryType
            << "," << query.Query.ConnectId << "\r\n";

    Retcode_T rc = AtQuectel_QueryQISTATE(this->t, &query, respArray, expRespLength, &respLength);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expRespLength, respLength);
    for (size_t i = 0; i < respLength; ++i)
    {
        ASSERT_EQ(expRespArray[i].ConnectId, respArray[i].ConnectId);
        ASSERT_EQ(expRespArray[i].ServiceType, respArray[i].ServiceType);
        ASSERT_EQ(expRespArray[i].IpAddress.Type, respArray[i].IpAddress.Type);
        ASSERT_EQ(0, memcmp(expRespArray[i].IpAddress.Address.IPv4,
                            respArray[i].IpAddress.Address.IPv4,
                            (expRespArray[i].IpAddress.Type == ATQUECTEL_ADDRESSTYPE_IPV4)
                                ? sizeof(AtQuectel_Address_T::Address.IPv4)
                                : sizeof(AtQuectel_Address_T::Address.IPv6)));
        ASSERT_EQ(expRespArray[i].RemotePort, respArray[i].RemotePort);
        ASSERT_EQ(expRespArray[i].LocalPort, respArray[i].LocalPort);
        ASSERT_EQ(expRespArray[i].SocketState, respArray[i].SocketState);
        ASSERT_EQ(expRespArray[i].ContextId, respArray[i].ContextId);
        ASSERT_EQ(expRespArray[i].ServerId, respArray[i].ServerId);
        ASSERT_EQ(expRespArray[i].AccessMode, respArray[i].AccessMode);
        ASSERT_EQ(expRespArray[i].AtPort, respArray[i].AtPort);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_SetQISEND : public TS_AtQuectel_GetQIACT
{
};

TEST_F(TS_AtQuectel_SetQISEND, Given__successful_send__When_sending_100_bytes_over_tcp_socket_2__Then__valid_message_written__return_ok)
{
    size_t expLength = 100;
    char expPayload[expLength];
    for (size_t i = 0; i < expLength; ++i)
    {
        expPayload[i] = i;
    }

    AtQuectel_QISEND_Set_T set;
    set.ConnectId = 2;
    set.Payload = expPayload;
    set.Length = expLength;
    set.RemoteIp.Type = ATQUECTEL_ADDRESSTYPE_INVALID;

    std::stringstream response;
    response << ">";
    response.write(expPayload, expLength);
    response << "\r\nSEND OK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISEND << "=" << set.ConnectId
            << "," << set.Length << "\r\n";
    expSent.write(expPayload, expLength);

    Retcode_T rc = AtQuectel_SetQISEND(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQISEND, Given__successful_send__When_sending_100_bytes_over_udpservice_socket_0_to_ipv4_remote__Then__valid_message_written__return_ok)
{
    size_t expLength = 100;
    char expPayload[expLength];
    for (size_t i = 0; i < expLength; ++i)
    {
        expPayload[i] = i;
    }

    AtQuectel_QISEND_Set_T set;
    set.ConnectId = 0;
    set.Payload = expPayload;
    set.Length = expLength;
    set.RemoteIp.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    set.RemoteIp.Address.IPv4[3] = 123;
    set.RemoteIp.Address.IPv4[2] = 64;
    set.RemoteIp.Address.IPv4[1] = 32;
    set.RemoteIp.Address.IPv4[0] = 16;
    set.RemotePort = 12345;

    std::stringstream response;
    response << ">";
    response.write(expPayload, expLength);
    response << "\r\nSEND OK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISEND << "=" << set.ConnectId
            << "," << set.Length << ",";
    Serialize(expSent, set.RemoteIp);
    expSent << "," << set.RemotePort << "\r\n";
    expSent.write(expPayload, expLength);

    Retcode_T rc = AtQuectel_SetQISEND(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_AtQuectel_SetQISEND, Given__successful_send__When_sending_100_bytes_over_udpservice_socket_1_to_ipv6_remote__Then__valid_message_written__return_ok)
{
    size_t expLength = 100;
    char expPayload[expLength];
    for (size_t i = 0; i < expLength; ++i)
    {
        expPayload[i] = i;
    }

    AtQuectel_QISEND_Set_T set;
    set.ConnectId = 1;
    set.Payload = expPayload;
    set.Length = expLength;
    set.RemoteIp.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    set.RemoteIp.Address.IPv6[7] = 0xfe01;
    set.RemoteIp.Address.IPv6[6] = 0xfefe;
    set.RemoteIp.Address.IPv6[5] = 0xefef;
    set.RemoteIp.Address.IPv6[4] = 0x1111;
    set.RemoteIp.Address.IPv6[3] = 0x2222;
    set.RemoteIp.Address.IPv6[2] = 0x3333;
    set.RemoteIp.Address.IPv6[1] = 0x4444;
    set.RemoteIp.Address.IPv6[0] = 0x5555;
    set.RemotePort = 54321;

    std::stringstream response;
    response << ">";
    response.write(expPayload, expLength);
    response << "\r\nSEND OK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISEND << "=" << set.ConnectId
            << "," << set.Length << ",";
    Serialize(expSent, set.RemoteIp);
    expSent << "," << set.RemotePort << "\r\n";
    expSent.write(expPayload, expLength);

    Retcode_T rc = AtQuectel_SetQISEND(this->t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_AtQuectel_QueryQISEND : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_AtQuectel_QueryQISEND, Given__1000_bytes_sent__900_acknowledged__100_unacknowledged__When__reading_send_statistics_of_socket_1__Then__valid_message_written__obtain_result__return_ok)
{
    size_t expTotalSendLength = 1000;
    size_t expAcknowledgedBytes = 900;
    size_t expUnacknowledgedBytes = 100;

    AtQuectel_QISEND_Query_T query;
    query.ConnectId = 1;

    AtQuectel_QISEND_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(resp));

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QISEND << ": " << expTotalSendLength << "," << expAcknowledgedBytes << "," << expUnacknowledgedBytes << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QISEND << "=" << query.ConnectId << ",0\r\n";

    Retcode_T rc = AtQuectel_QueryQISEND(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());

    ASSERT_EQ(expTotalSendLength, resp.TotalSendLength);
    ASSERT_EQ(expAcknowledgedBytes, resp.AcknowledgedBytes);
    ASSERT_EQ(expUnacknowledgedBytes, resp.UnacknowledgedBytes);
}

class TS_AtQuectel_QueryQIRD : public TS_AtQuectel_GetQIACT
{
};

TEST_F(TS_AtQuectel_QueryQIRD, Given__tcp_socket__100_bytes_payload__When__reading_payload_from_socket_1__Then__valid_message_written__obtain_payload__return_ok)
{
    size_t expReadLength = 100;
    char expPayload[expReadLength];
    for (size_t i = 0; i < expReadLength; ++i)
    {
        expPayload[i] = i;
    }
    char actPayload[expReadLength];
    memset(actPayload, 0xAA, sizeof(expReadLength));

    AtQuectel_QIRD_Query_T query;
    query.ConnectId = 1;
    query.ReadLength = expReadLength;

    AtQuectel_QIRD_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(resp));
    resp.Payload.Data = actPayload;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QIRD << ": " << expReadLength << "\r\n";
    response.write(expPayload, expReadLength);
    response << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIRD << "=" << query.ConnectId << "," << query.ReadLength << "\r\n";

    Retcode_T rc = AtQuectel_QueryQIRD(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());

    ASSERT_EQ(expReadLength, resp.Payload.ReadActualLength);
    ASSERT_EQ(0, memcmp(expPayload, resp.Payload.Data, resp.Payload.ReadActualLength));
    ASSERT_EQ(ATQUECTEL_ADDRESSTYPE_INVALID, resp.Payload.RemoteIp.Type);
}

TEST_F(TS_AtQuectel_QueryQIRD, Given__udpservice_socket__1500_bytes_payload_from_ipv4__When__reading_payload_from_socket_0__Then__valid_message_written__obtain_payload__return_ok)
{
    size_t expReadLength = 1500;
    char expPayload[expReadLength];
    for (size_t i = 0; i < expReadLength; ++i)
    {
        expPayload[i] = i;
    }
    AtQuectel_Address_T expRemoteIp;
    expRemoteIp.Type = ATQUECTEL_ADDRESSTYPE_IPV4;
    expRemoteIp.Address.IPv4[3] = 128;
    expRemoteIp.Address.IPv4[2] = 3;
    expRemoteIp.Address.IPv4[1] = 2;
    expRemoteIp.Address.IPv4[0] = 1;
    uint16_t expRemotePort = 12345;
    char actPayload[expReadLength];
    memset(actPayload, 0xAA, sizeof(expReadLength));

    AtQuectel_QIRD_Query_T query;
    query.ConnectId = 0;
    query.ReadLength = expReadLength;

    AtQuectel_QIRD_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(resp));
    resp.Payload.Data = actPayload;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QIRD << ": " << expReadLength << ",";
    Serialize(response, expRemoteIp);
    response << "," << expRemotePort << "\r\n";
    response.write(expPayload, expReadLength);
    response << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIRD << "=" << query.ConnectId << "," << query.ReadLength << "\r\n";

    Retcode_T rc = AtQuectel_QueryQIRD(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());

    ASSERT_EQ(expReadLength, resp.Payload.ReadActualLength);
    ASSERT_EQ(0, memcmp(expPayload, resp.Payload.Data, resp.Payload.ReadActualLength));
    ASSERT_EQ(ATQUECTEL_ADDRESSTYPE_IPV4, resp.Payload.RemoteIp.Type);
    ASSERT_EQ(0, memcmp(expRemoteIp.Address.IPv4, resp.Payload.RemoteIp.Address.IPv4, sizeof(AtQuectel_Address_T::Address.IPv4)));
    ASSERT_EQ(expRemotePort, resp.Payload.RemotePort);
}

TEST_F(TS_AtQuectel_QueryQIRD, Given__udpservice_socket__1500_bytes_payload_from_ipv6__When__reading_payload_from_socket_0__Then__valid_message_written__obtain_payload__return_ok)
{
    size_t expReadLength = 1500;
    char expPayload[expReadLength];
    for (size_t i = 0; i < expReadLength; ++i)
    {
        expPayload[i] = i;
    }
    AtQuectel_Address_T expRemoteIp;
    expRemoteIp.Type = ATQUECTEL_ADDRESSTYPE_IPV6;
    expRemoteIp.Address.IPv6[7] = 0x1234;
    expRemoteIp.Address.IPv6[6] = 0x4321;
    expRemoteIp.Address.IPv6[5] = 0x0001;
    expRemoteIp.Address.IPv6[4] = 0x1000;
    expRemoteIp.Address.IPv6[3] = 0xfefe;
    expRemoteIp.Address.IPv6[2] = 0xefef;
    expRemoteIp.Address.IPv6[1] = 0x1111;
    expRemoteIp.Address.IPv6[0] = 0x2222;
    uint16_t expRemotePort = 54321;
    char actPayload[expReadLength];
    memset(actPayload, 0xAA, sizeof(expReadLength));

    AtQuectel_QIRD_Query_T query;
    query.ConnectId = 0;
    query.ReadLength = expReadLength;

    AtQuectel_QIRD_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(resp));
    resp.Payload.Data = actPayload;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QIRD << ": " << expReadLength << ",";
    Serialize(response, expRemoteIp);
    response << "," << expRemotePort << "\r\n";
    response.write(expPayload, expReadLength);
    response << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIRD << "=" << query.ConnectId << "," << query.ReadLength << "\r\n";

    Retcode_T rc = AtQuectel_QueryQIRD(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());

    ASSERT_EQ(expReadLength, resp.Payload.ReadActualLength);
    ASSERT_EQ(0, memcmp(expPayload, resp.Payload.Data, resp.Payload.ReadActualLength));
    ASSERT_EQ(ATQUECTEL_ADDRESSTYPE_IPV6, resp.Payload.RemoteIp.Type);
    ASSERT_EQ(0, memcmp(expRemoteIp.Address.IPv6, resp.Payload.RemoteIp.Address.IPv6, sizeof(AtQuectel_Address_T::Address.IPv6)));
    ASSERT_EQ(expRemotePort, resp.Payload.RemotePort);
}

TEST_F(TS_AtQuectel_QueryQIRD, Given__100_bytes_received_total__50_bytes_read_total__10_bytes_unread__When__reading_receive_statistics_from_socket_2__Then__valid_message_written__obtain_statistics__return_ok)
{
    size_t expTotalReceiveLength = 100;
    size_t expHaveReadLength = 50;
    size_t expUnreadLength = 10;

    AtQuectel_QIRD_Query_T query;
    query.ConnectId = 2;
    query.ReadLength = 0;

    AtQuectel_QIRD_QueryResponse_T resp;
    memset(&resp, 0xAA, sizeof(resp));

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_QIRD << ": " << expTotalReceiveLength
             << "," << expHaveReadLength << "," << expUnreadLength
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_QIRD << "=" << query.ConnectId << "," << query.ReadLength << "\r\n";

    Retcode_T rc = AtQuectel_QueryQIRD(this->t, &query, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());

    ASSERT_EQ(expTotalReceiveLength, resp.Statistics.TotalReceiveLength);
    ASSERT_EQ(expHaveReadLength, resp.Statistics.HaveReadLength);
    ASSERT_EQ(expUnreadLength, resp.Statistics.UnreadLength);
}

#endif /* CELLULAR_VARIANT_QUECTEL */
