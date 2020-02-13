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

#include <gtest.h>
#include <fff.h>

FFF_DEFINITION_BLOCK_START
extern "C"
{
#include "Kiso_CellularModules.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_AT3GPP27007
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
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_LeadingZeros_Pass)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.002.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_Normal_Pass)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
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
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_LeadingZeros_Pass)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
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
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_Quotes_Pass)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "\"192.168.2.100\"";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_OK, retcode);
    EXPECT_EQ(192, addr.Address.IPv4[3]);
    EXPECT_EQ(168, addr.Address.IPv4[2]);
    EXPECT_EQ(2, addr.Address.IPv4[1]);
    EXPECT_EQ(100, addr.Address.IPv4[0]);
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv6_Quotes_Pass)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
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
    EXPECT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6, addr.Type);
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidOctet1_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "300.168.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv6_InvalidOctet1_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "300.128.1.2.3.4.5.6.7.8.9.10.11.12.19.55";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidOctet2_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "200.1680.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv6_InvalidOctet2_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "254.128.1.2.3.4.5.6.7.8.9.10.11.12.19.1055";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_MissingOctet_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "100.168.2";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, NullBuff_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;

    retcode = ExtractPdpAddress(NULL, 0, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, ZeroLen_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "200.1680.2.100";

    retcode = ExtractPdpAddress(addrStr, 0, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_LeadingZero_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "20.0016.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, BuffTooLong_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "192.168.2.100";

    retcode = ExtractPdpAddress(addrStr, MAX_IP_STR_LENGTH + 1, &addr);

    EXPECT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidChar1_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "192.+.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

TEST_F(TS_ExtractPdpAddress, IPv4_InvalidChar2_Fail)
{
    Retcode_T retcode;
    At3Gpp27007_CGPADDR_Address_T addr;
    const char *addrStr = "192.a.2.100";

    retcode = ExtractPdpAddress(addrStr, strlen(addrStr), &addr);

    EXPECT_EQ(RETCODE_FAILURE, Retcode_GetCode(retcode));
}

class TS_At3Gpp27007_SetCREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCREG, Given____When__set_CREG_disabled_URC_mode__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);
    At3Gpp27007_CXREG_Set_T set;
    set.N = AT3GPP27007_CXREG_N_DISABLED;
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCREG(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCREG, Given____When__set_CREG_some_URC_mode__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);
    At3Gpp27007_CXREG_Set_T set;
    set.N = (At3Gpp27007_CXREG_N_T)random(AT3GPP27007_CXREG_N_DISABLED, AT3GPP27007_CXREG_N_URC_LOC_PSM_CAUSE);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCREG(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCGREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCGREG, Given____When__set_CGREG_some_URC_mode__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);
    At3Gpp27007_CXREG_Set_T set;
    set.N = (At3Gpp27007_CXREG_N_T)random(AT3GPP27007_CXREG_N_DISABLED, AT3GPP27007_CXREG_N_URC_LOC_PSM_CAUSE);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGREG(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCEREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCEREG, Given____When__set_CGREG_some_URC_mode__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);
    At3Gpp27007_CXREG_Set_T set;
    set.N = (At3Gpp27007_CXREG_N_T)random(AT3GPP27007_CXREG_N_DISABLED, AT3GPP27007_CXREG_N_URC_LOC_PSM_CAUSE);
    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCEREG(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCOPS : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCOPS, Given__automatic_mode__When__setting_COPS__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_AUTOMATIC;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_COPS << "=" << set.Mode << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCOPS, Given__deregister_mode__When__setting_COPS__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_DEREGISTER;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_COPS << "=" << set.Mode << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCOPS, Given__manual_mode__numeric_format__no_act__When__setting_COPS__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_MANUAL;
    set.Format = AT3GPP27007_COPS_FORMAT_NUMERIC;
    set.Oper.Numeric = 0xABAB;
    set.AcT = AT3GPP27007_COPS_ACT_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_COPS << "=" << set.Mode << "," << set.Format << ",\"" << set.Oper.Numeric << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCOPS, Given__set_format_mode__numeric_format__When__setting_COPS__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_SET_FORMAT_ONLY;
    set.Format = AT3GPP27007_COPS_FORMAT_NUMERIC;
    set.Oper.Numeric = 0xABAB;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_COPS << "=" << set.Mode << "," << set.Format << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCOPS, Given__manual_then_automatic_mode__When__setting_COPS__Then__return_not_supported)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC;
    set.Format = AT3GPP27007_COPS_FORMAT_NUMERIC;
    set.Oper.Numeric = 0xABAB;
    set.AcT = AT3GPP27007_COPS_ACT_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_COPS << "=" << set.Mode << "," << set.Format << ",\"" << set.Oper.Numeric << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCOPS, Given__invalid_mode__When__setting_COPS__Then__return_invalid_parameter_error)
{
    At3Gpp27007_COPS_Set_T set;
    memset(&set, 0xAA, sizeof(set));
    set.Mode = AT3GPP27007_COPS_MODE_INVALID;

    Retcode_T rc = At3Gpp27007_SetCOPS(t, &set);

    ASSERT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

class TS_At3Gpp27007_SetCGDCONT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_x25_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_X25;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_OSPIH_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_OSPIH;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_PPP_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_PPP;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_nonIP_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_NONIP;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_ethernet_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_ETHERNET;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_unstructured_and_valid_APN__Then__return_not_supported)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_UNSTRUCTURED;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_NOT_SUPPORTED, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_invalid_pdp_type_and_valid_APN__Then__return_invalid_parameter_error)
{
    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_INVALID;

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_invalid_pdp_type_and_APN_nullptr__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = nullptr;
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGDCONT << "=" << (unsigned int)set.Cid << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_IP_and_valid_APN__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_IP;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGDCONT << "=" << (unsigned int)set.Cid << ",\"" << ARG_CGDCONT_PDPTYPE_IP << "\",\"" << set.Apn << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_IPv6_and_valid_APN__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_IPV6;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGDCONT << "=" << (unsigned int)set.Cid << ",\"" << ARG_CGDCONT_PDPTYPE_IPV6 << "\",\"" << set.Apn << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_IPv4v6_and_valid_APN__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = "some.apn.net";
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_IPV4V6;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGDCONT << "=" << (unsigned int)set.Cid << ",\"" << ARG_CGDCONT_PDPTYPE_IPV4V6 << "\",\"" << set.Apn << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGDCONT, Given____When__setting_CGDCONT_to_valid_cid_and_pdp_type_IPv4v6_and_APN_nullptr___Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGDCONT_Set_T set;
    set.Cid = 0;
    set.Apn = nullptr;
    set.PdpType = AT3GPP27007_CGDCONT_PDPTYPE_IPV4V6;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGDCONT << "=" << (unsigned int)set.Cid << ",\"" << ARG_CGDCONT_PDPTYPE_IPV4V6 << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCGDCONT(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCGACT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCGACT, Given____valid_cid__When__setting_CGACT_to_activated__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGACT_Set_T set;
    set.State = AT3GPP27007_CGACT_STATE_ACTIVATED;
    set.Cid = 0;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGACT << "=" << set.State << "," << (unsigned int)set.Cid << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGACT(t, &set);

    ASSERT_EQ(RETCODE_OK, Retcode_GetCode(rc));
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGACT, Given____valid_cid__When__setting_CGACT_to_deactivated__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CGACT_Set_T set;
    set.State = AT3GPP27007_CGACT_STATE_DEACTIVATED;
    set.Cid = 3;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGACT << "=" << set.State << "," << (unsigned int)set.Cid << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGACT(t, &set);

    ASSERT_EQ(RETCODE_OK, Retcode_GetCode(rc));
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCGPADDR : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCGPADDR, Given__same_cid_as_in_request__valid_IPv4_address_configured__When__setting_CGPADDR__Then__cids_matching__valid_IPv4_parsed__valid_message_written__return_ok)
{
    At3Gpp27007_CGPADDR_Query_T set;
    set.Cid = 1;
    At3Gpp27007_CGPADDR_QueryResponse_T resp;
    resp.Cid = !set.Cid;
    memset(&resp.PdpAddr.Address, 0U, sizeof(resp.PdpAddr.Address));
    resp.PdpAddr.Type = AT3GPP27007_CGPADDR_ADDRESSTYPE_INVALID;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGPADDR << ": " << (unsigned int)set.Cid << ",\"";
    for (size_t i = 0; i < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv4); ++i)
    {
        response << (unsigned int)i + 1;
        if (i + 1 < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv4))
            response << ".";
    }
    response << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGPADDR << "=" << (unsigned int)set.Cid << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGPADDR(t, &set, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(set.Cid, resp.Cid);
    ASSERT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4, resp.PdpAddr.Type);
    for (size_t i = 0; i < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv4); ++i)
    {
        ASSERT_EQ(i + 1, resp.PdpAddr.Address.IPv4[sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv4) - i - 1]);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCGPADDR, Given__same_cid_as_in_request__valid_IPv6_address_configured__When__setting_CGPADDR__Then__cids_matching__valid_IPv6_parsed__valid_message_written__return_ok)
{
    At3Gpp27007_CGPADDR_Query_T set;
    set.Cid = 3;
    At3Gpp27007_CGPADDR_QueryResponse_T resp;
    resp.Cid = !set.Cid;
    memset(&resp.PdpAddr.Address, 0U, sizeof(resp.PdpAddr.Address));
    resp.PdpAddr.Type = AT3GPP27007_CGPADDR_ADDRESSTYPE_INVALID;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGPADDR << ": " << (unsigned int)set.Cid << ",\"";
    for (size_t i = 0; i < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv6); ++i)
    {
        response << (unsigned int)i + 1;
        if (i + 1 < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv6))
            response << ".";
    }
    response << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGPADDR << "=" << (unsigned int)set.Cid << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCGPADDR(t, &set, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(set.Cid, resp.Cid);
    ASSERT_EQ(AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6, resp.PdpAddr.Type);
    for (size_t i = 0; i < sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv6); ++i)
    {
        ASSERT_EQ(i + 1, resp.PdpAddr.Address.IPv6[sizeof(At3Gpp27007_CGPADDR_Address_T::Address.IPv6) - i - 1]);
    }
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCMEE : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCMEE, Given____When__setting_CMEE_to_no_error_reporting__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CMEE_Set_T set;
    set.N = AT3GPP27007_CMEE_N_DISABLED;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CMEE << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCMEE(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCMEE, Given____When__setting_CMEE_to_numeric_error_reporting__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CMEE_Set_T set;
    set.N = AT3GPP27007_CMEE_N_NUMERIC;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CMEE << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCMEE(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCMEE, Given____When__setting_CMEE_to_verbose_error_reporting__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CMEE_Set_T set;
    set.N = AT3GPP27007_CMEE_N_VERBOSE;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CMEE << "=" << set.N << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCMEE(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCMEE, Given____When__setting_CMEE_to_invalid_value__Then__no_message_written__return_invalid_parameter_error)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CMEE_Set_T set;
    set.N = AT3GPP27007_CMEE_N_INVALID;

    Retcode_T rc = At3Gpp27007_SetCMEE(t, &set);

    ASSERT_EQ(RETCODE_INVALID_PARAM, Retcode_GetCode(rc));
    ASSERT_TRUE(WrittenTransceiverData.str().empty());
}

class TS_At3Gpp27007_SetCPIN : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCPIN, Given__correct_pin__When__setting_CPIN__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CPIN_Set_T set;
    set.Pin = "1234";
    set.NewPin = NULL;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CPIN << "=\"" << set.Pin << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCPIN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCPIN, Given__correct_pin__new_pin__When__setting_CPIN__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CPIN_Set_T set;
    set.Pin = "1234";
    set.NewPin = "4321";

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CPIN << "=\"" << set.Pin << "\",\"" << set.NewPin << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCPIN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCPIN, Given__wrong_pin__When__setting_CPIN__Then__valid_message_written__return_cellular_error_response)
{
    std::string response = "\r\nERROR\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CPIN_Set_T set;
    set.Pin = "4321";
    set.NewPin = NULL;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CPIN << "=\"" << set.Pin << "\"\r\n";

    Retcode_T rc = At3Gpp27007_SetCPIN(t, &set);

    ASSERT_EQ(RETCODE_CELLULAR_RESPONDED_ERROR, Retcode_GetCode(rc));
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_ExecuteAT : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_ExecuteAT, Given____When__activating_AT__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    std::stringstream expSent;
    expSent << "AT\r\n";

    Retcode_T rc = At3Gpp27007_ExecuteAT(t);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_ExecuteATE : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_ExecuteATE, Given____When__activating_AT_echo__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    std::stringstream expSent;
    expSent << "ATE1\r\n";

    Retcode_T rc = At3Gpp27007_ExecuteATE(t, true);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_ExecuteATE, Given____When__deactivating_AT_echo__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    std::stringstream expSent;
    expSent << "ATE0\r\n";

    Retcode_T rc = At3Gpp27007_ExecuteATE(t, false);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_SetCFUN : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_SetCFUN, Given____When__setting_CFUN_to_minimum_functionality_without_reset_option__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CFUN_Set_T set;
    set.Fun = AT3GPP27007_CFUN_FUN_MINIMUM;
    set.Rst = AT3GPP27007_CFUN_RST_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "=" << set.Fun << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCFUN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCFUN, Given____When__setting_CFUN_to_minimum_functionality_with_reset__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CFUN_Set_T set;
    set.Fun = AT3GPP27007_CFUN_FUN_MINIMUM;
    set.Rst = AT3GPP27007_CFUN_RST_RESET;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "=" << set.Fun << "," << set.Rst << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCFUN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCFUN, Given____When__setting_CFUN_to_minimum_functionality_without_reset__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CFUN_Set_T set;
    set.Fun = AT3GPP27007_CFUN_FUN_MINIMUM;
    set.Rst = AT3GPP27007_CFUN_RST_NORESET;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "=" << set.Fun << "," << set.Rst << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCFUN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

TEST_F(TS_At3Gpp27007_SetCFUN, Given____When__setting_CFUN_to_full_functionality_without_reset_option__Then__valid_message_written__return_ok)
{
    std::string response = "\r\nOK\r\n";
    AtTransceiver_Feed(t, response.c_str(), response.length(), nullptr);

    At3Gpp27007_CFUN_Set_T set;
    set.Fun = AT3GPP27007_CFUN_FUN_FULL;
    set.Rst = AT3GPP27007_CFUN_RST_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "=" << set.Fun << "\r\n";

    Retcode_T rc = At3Gpp27007_SetCFUN(t, &set);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
}

class TS_At3Gpp27007_GetCFUN : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_GetCFUN, Given__minimum_functionality__When__getting_CFUN__Then__valid_message_written__obtain_functionality_level__return_ok)
{
    At3Gpp27007_CFUN_Fun_T expFun = AT3GPP27007_CFUN_FUN_MINIMUM;

    std::stringstream response;
    response << "AT" << CMD_SEPARATOR << CMD_CFUN << ": " << expFun;
    response << "\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CFUN_GetResponse_T resp;
    resp.Fun = AT3GPP27007_CFUN_FUN_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCFUN(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expFun, resp.Fun);
}

TEST_F(TS_At3Gpp27007_GetCFUN, Given__full_functionality__When__getting_CFUN__Then__valid_message_written__obtain_functionality_level__return_ok)
{
    At3Gpp27007_CFUN_Fun_T expFun = AT3GPP27007_CFUN_FUN_FULL;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CFUN << ": " << expFun << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CFUN_GetResponse_T resp;
    resp.Fun = AT3GPP27007_CFUN_FUN_INVALID;

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CFUN << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCFUN(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expFun, resp.Fun);
}

class TS_At3Gpp27007_GetCREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_GetCREG, Given__disabled_URC_mode__not_registered__When__getting_CREG__Then__valid_message_written__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_NOT;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__disabled_URC_mode__registered_to_home__When__getting_CREG__Then__valid_message_written__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__simple_URC_mode__registered_to_home__When__getting_CREG__Then__valid_message_written__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__detailed_URC_mode__no_registration__invalid_location_data__When__getting_CREG__Then__obtain_mode_and_registration_state__set_invalid_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = AT3GPP27007_INVALID_LAC;
    uint32_t expCi = AT3GPP27007_INVALID_CI;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__detailed_URC_mode__no_registration__empty_location_data__When__getting_CREG__Then__obtain_mode_and_registration_state__set_invalid_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = AT3GPP27007_INVALID_LAC;
    uint32_t expCi = AT3GPP27007_INVALID_CI;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << ","
             << ","
             << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__detailed_URC_mode__registered_to_home__valid_location_data__GSM_access__When__getting_CREG__Then__obtain_mode_and_registration_state__set_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = 0xABCD;
    uint32_t expCi = 0x89ABCDEF;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"," << expAcT << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCREG, Given__detailed_URC_mode__registered_to_home__valid_short_location_data__GSM_access__When__getting_CREG__Then__obtain_mode_and_registration_state__set_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = 0xAB;
    uint32_t expCi = 0x89ABCD;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"," << expAcT << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
}

class TS_At3Gpp27007_GetCGREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_GetCGREG, Given__disabled_URC_mode__not_registered__When__getting_CGREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_NOT;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
    ASSERT_EQ(AT3GPP27007_INVALID_RAC, resp.Rac);
}

TEST_F(TS_At3Gpp27007_GetCGREG, Given__disabled_URC_mode__registered_to_home__When__getting_CGREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
    ASSERT_EQ(AT3GPP27007_INVALID_RAC, resp.Rac);
}

TEST_F(TS_At3Gpp27007_GetCGREG, Given__simple_URC_mode__registered_to_home__When__getting_CGREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
    ASSERT_EQ(AT3GPP27007_INVALID_RAC, resp.Rac);
}

TEST_F(TS_At3Gpp27007_GetCGREG, Given__detailed_URC_mode__no_registration__invalid_location_data__When__getting_CGREG__Then__obtain_mode_and_registration_state__set_invalid_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_NOT;
    uint16_t expLac = AT3GPP27007_INVALID_LAC;
    uint32_t expCi = AT3GPP27007_INVALID_CI;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
    ASSERT_EQ(AT3GPP27007_INVALID_RAC, resp.Rac);
}

TEST_F(TS_At3Gpp27007_GetCGREG, Given__detailed_URC_mode__registered_to_home__valid_location_data__GSM_access__When__getting_CGREG__Then__obtain_mode_and_registration_state__set_location_and_routing_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = 0xABCD;
    uint32_t expCi = 0x89ABCDEF;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;
    uint8_t expRac = 0x14;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"," << std::dec << expAcT << ",\"" << std::hex << (unsigned int)expRac << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
    ASSERT_EQ(expRac, resp.Rac);
}

TEST_F(TS_At3Gpp27007_GetCGREG, Given__detailed_URC_mode__registered_to_home__valid_short_location_data__GSM_access__When__getting_CGREG__Then__obtain_mode_and_registration_state__set_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expLac = 0xAB;
    uint32_t expCi = 0x89ABCD;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;
    uint8_t expRac = 0x14;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CGREG << ": " << expN << "," << expStat << ",\"" << std::hex << expLac << "\",\"" << expCi << "\"," << std::dec << expAcT << ",\"" << std::hex << (unsigned int)expRac << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CGREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CGREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCGREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expLac, resp.Lac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
    ASSERT_EQ(expRac, resp.Rac);
}
class TS_At3Gpp27007_GetCEREG : public TS_ReadableAndWritableTransceiver
{
};

TEST_F(TS_At3Gpp27007_GetCEREG, Given__disabled_URC_mode__not_registered__When__getting_CEREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_NOT;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_INVALID_TAC, resp.Tac);
    ASSERT_EQ(AT3GPP27007_INVALID_CI, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCEREG, Given__disabled_URC_mode__registered_to_home__When__getting_CEREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_DISABLED;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_INVALID_TAC, resp.Tac);
    ASSERT_EQ(AT3GPP27007_INVALID_CI, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCEREG, Given__simple_URC_mode__registered_to_home__When__getting_CEREG__Then__obtain_mode_and_registration_state__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(AT3GPP27007_INVALID_TAC, resp.Tac);
    ASSERT_EQ(AT3GPP27007_INVALID_CI, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCEREG, Given__detailed_URC_mode__no_registration__invalid_location_data__When__getting_CEREG__Then__obtain_mode_and_registration_state__set_invalid_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_NOT;
    uint16_t expTac = AT3GPP27007_INVALID_TAC;
    uint32_t expCi = AT3GPP27007_INVALID_CI;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << ",\"" << std::hex << expTac << "\",\"" << expCi << "\"\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expTac, resp.Tac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(AT3GPP27007_CXREG_ACT_INVALID, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCEREG, Given__detailed_URC_mode__registered_to_home__valid_location_data__GSM_access__When__getting_CEREG__Then__obtain_mode_and_registration_state__set_location_and_routing_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expTac = 0xABCD;
    uint32_t expCi = 0x89ABCDEF;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << ",\"" << std::hex << expTac << "\",\"" << expCi << "\"," << expAcT << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expTac, resp.Tac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
}

TEST_F(TS_At3Gpp27007_GetCEREG, Given__detailed_URC_mode__registered_to_home__valid_short_location_data__GSM_access__When__getting_CEREG__Then__obtain_mode_and_registration_state__set_location_data__return_ok)
{
    At3Gpp27007_CXREG_N_T expN = AT3GPP27007_CXREG_N_URC_LOC;
    At3Gpp27007_CXREG_Stat_T expStat = AT3GPP27007_CXREG_STAT_HOME;
    uint16_t expTac = 0xAB;
    uint32_t expCi = 0x89ABCD;
    At3Gpp27007_CXREG_AcT_T expAcT = AT3GPP27007_CXREG_ACT_GSM;

    std::stringstream response;
    response << CMD_SEPARATOR << CMD_CEREG << ": " << expN << "," << expStat << ",\"" << std::hex << expTac << "\",\"" << expCi << "\"," << expAcT << "\r\n\r\nOK\r\n";
    std::string responseStr = response.str();
    AtTransceiver_Feed(t, responseStr.c_str(), responseStr.length(), nullptr);

    At3Gpp27007_CEREG_GetResponse_T resp;
    memset(&resp, 0, sizeof(resp));

    std::stringstream expSent;
    expSent << "AT" << CMD_SEPARATOR << CMD_CEREG << "?\r\n";

    Retcode_T rc = At3Gpp27007_GetCEREG(t, &resp);

    ASSERT_EQ(RETCODE_OK, rc);
    ASSERT_EQ(expSent.str(), WrittenTransceiverData.str());
    ASSERT_EQ(expN, resp.N);
    ASSERT_EQ(expStat, resp.Stat);
    ASSERT_EQ(expTac, resp.Tac);
    ASSERT_EQ(expCi, resp.Ci);
    ASSERT_EQ(expAcT, resp.AcT);
}
