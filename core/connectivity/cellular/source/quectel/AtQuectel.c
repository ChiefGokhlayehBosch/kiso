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

#include "Quectel.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ATQUECTEL

#include "Kiso_CellularConfig.h"
#ifdef CELLULAR_VARIANT_QUECTEL

#include "AtQuectel.h"

#include "Kiso_Cellular.h"
#include "AtTransceiver.h"

#include "Kiso_Retcode.h"

#include <stdio.h>

#define ATQUECTEL_MAXIPSTRLENGTH (39U) /* "255.255.255.255" or "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF" */
#define ATQUECTEL_IPV4GROUPCOUNT (4U)
#define ATQUECTEL_IPV6GROUPCOUNT (8U)

#define SHORT_TIMEOUT (120U)

#define CMD_QCFG "QCFG"
#define CMD_QURCCFG "QURCCFG"
#define CMD_QCCID "QCCID"
#define CMD_QINDCFG "QINDCFG"
#define CMD_QINISTAT "QINISTAT"
#define CMD_QICSGP "QICSGP"
#define CMD_QIACT "QIACT"
#define CMD_QIDEACT "QIDEACT"
#define CMD_QIOPEN "QIOPEN"
#define CMD_QICLOSE "QICLOSE"
#define CMD_QISTATE "QISTATE"
#define CMD_QISEND "QISEND"
#define CMD_QIRD "QIRD"

#define CMD_SEPARATOR "+"
#define ARG_SEPARATOR ","

#define ARG_NWSCANMODE "nwscanmode"
#define ARG_NWSCANSEQ "nwscanseq"
#define ARG_IOTOPMODE "iotopmode"

#define ARG_URCPORT "urcport"
#define ARG_USBAT "usbat"
#define ARG_USBMODEM "usbmodem"
#define ARG_UART1 "uart1"

#define ARG_ALL "all"
#define ARG_CSQ "csq"
#define ARG_SMSFULL "smsfull"
#define ARG_RING "ring"
#define ARG_SMSINCOMING "smsincoming"

#define ARG_TCP "TCP"
#define ARG_UDP "UDP"
#define ARG_TCPLISTENER "TCP LISTENER"
#define ARG_TCPINCOMING "TCP INCOMING"
#define ARG_UDPSERVICE "UDP SERVICE"

#define ATQUECTEL_MIN(a, b) ((a) > (b) ? (b) : (a))
#define ATQUECTEL_MAX(a, b) ((a) < (b) ? (b) : (a))
#define ATQUECTEL_MAX3(a, b, c) ATQUECTEL_MAX(ATQUECTEL_MAX((a), (b)), (c))
#define ATQUECTEL_MAX4(a, b, c, d) ATQUECTEL_MAX(ATQUECTEL_MAX3((a), (b), (c)), (d))
#define ATQUECTEL_MAX5(a, b, c, d, e) ATQUECTEL_MAX(ATQUECTEL_MAX4((a), (b), (c), (d)), (e))
#define ATQUECTEL_MAX6(a, b, c, d, e, f) ATQUECTEL_MAX(ATQUECTEL_MAX5((a), (b), (c), (d), (e)), (f))
#define ATQUECTEL_MAX7(a, b, c, d, e, f, g) ATQUECTEL_MAX(ATQUECTEL_MAX6((a), (b), (c), (d), (e), (f)), (g))
#define ATQUECTEL_MAX8(a, b, c, d, e, f, g, h) ATQUECTEL_MAX(ATQUECTEL_MAX7((a), (b), (c), (d), (e), (f), (g)), (h))
#define ATQUECTEL_MAX9(a, b, c, d, e, f, g, h, i) ATQUECTEL_MAX(ATQUECTEL_MAX8((a), (b), (c), (d), (e), (f), (g), (h)), (i))

struct AtQuectel_QCFG_Handler_S
{
    AtQuectel_QCFG_Setting_T Setting;
    const char *String;
    Retcode_T (*HandleConfigWrite)(struct AtTransceiver_S *, const AtQuectel_QCFG_Set_T *);
    Retcode_T (*HandleConfigRead)(struct AtTransceiver_S *, AtQuectel_QCFG_QueryResponse_T *);
};

struct AtQuectel_QURCCFG_Handler_S
{
    AtQuectel_QURCCFG_Setting_T Setting;
    const char *String;
    Retcode_T (*HandleConfigWrite)(struct AtTransceiver_S *, const AtQuectel_QURCCFG_Set_T *);
    Retcode_T (*HandleConfigRead)(struct AtTransceiver_S *, AtQuectel_QURCCFG_QueryResponse_T *);
};

struct AtQuectel_QINDCFG_Mapping_S
{
    AtQuectel_QINDCFG_UrcType_T Enum;
    const char *String;
};

#define ATQUECTEL_QCFG_HANDLER_TABLE(EXPANDER)                                                               \
    EXPANDER(ATQUECTEL_QCFG_SETTING_NWSCANMODE, ARG_NWSCANMODE, WriteNwScanModeConfig, ReadNwScanModeConfig) \
    EXPANDER(ATQUECTEL_QCFG_SETTING_NWSCANSEQ, ARG_NWSCANSEQ, WriteNwScanSeqConfig, ReadNwScanSeqConfig)     \
    EXPANDER(ATQUECTEL_QCFG_SETTING_IOTOPMODE, ARG_IOTOPMODE, WriteIoTOpModeConfig, ReadIoTOpModeConfig)

#define ATQUECTEL_QURCCFG_HANDLER_TABLE(EXPANDER) \
    EXPANDER(ATQUECTEL_QURCCFG_SETTING_URCPORT, ARG_URCPORT, WriteUrcPortConfig, ReadUrcPortConfig)

#define ATQUECTEL_QCFG_HANDLER_INLINEINIT(SETTING, STRING, WRITE_HANDLER, READ_HANDLER) \
    {SETTING, STRING, WRITE_HANDLER, READ_HANDLER},
#define ATQUECTEL_QURCCFG_HANDLER_INLINEINIT(SETTING, STRING, WRITE_HANDLER, READ_HANDLER) \
    {SETTING, STRING, WRITE_HANDLER, READ_HANDLER},
#define ATQUECTEL_QCFG_HANDLER_FUNCDECL(SETTING, STRING, WRITE_HANDLER, READ_HANDLER)       \
    static Retcode_T WRITE_HANDLER(struct AtTransceiver_S *, const AtQuectel_QCFG_Set_T *); \
    static Retcode_T READ_HANDLER(struct AtTransceiver_S *, AtQuectel_QCFG_QueryResponse_T *);
#define ATQUECTEL_QURCCFG_HANDLER_FUNCDECL(SETTING, STRING, WRITE_HANDLER, READ_HANDLER)       \
    static Retcode_T WRITE_HANDLER(struct AtTransceiver_S *, const AtQuectel_QURCCFG_Set_T *); \
    static Retcode_T READ_HANDLER(struct AtTransceiver_S *, AtQuectel_QURCCFG_QueryResponse_T *);

#define ATQUECTEL_QCFG_HANDLER_ENUM(SETTING, STRING, WRITE_HANDLER, READ_HANDLER) __##SETTING,
#define ATQUECTEL_QURCCFG_HANDLER_ENUM(SETTING, STRING, WRITE_HANDLER, READ_HANDLER) __##SETTING,

enum
{
    ATQUECTEL_QCFG_HANDLER_TABLE(ATQUECTEL_QCFG_HANDLER_ENUM)

        QCFG_HANDLERS_MAX
};

enum
{
    ATQUECTEL_QURCCFG_HANDLER_TABLE(ATQUECTEL_QURCCFG_HANDLER_ENUM)

        QURCCFG_HANDLERS_MAX
};

ATQUECTEL_QCFG_HANDLER_TABLE(ATQUECTEL_QCFG_HANDLER_FUNCDECL)
ATQUECTEL_QURCCFG_HANDLER_TABLE(ATQUECTEL_QURCCFG_HANDLER_FUNCDECL)

static Retcode_T HandleCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E expectedCode);
static inline Retcode_T HandleCodeOk(struct AtTransceiver_S *t);
static Retcode_T FlushAndHandleCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E expectedCode);
static inline Retcode_T FlushAndHandleCodeOk(struct AtTransceiver_S *t);
static Retcode_T WriteQuectelAddress(struct AtTransceiver_S *t, const AtQuectel_Address_T *addr);
static Retcode_T ParseResponsesFromQISTATE(struct AtTransceiver_S *t, AtQuectel_QISTATE_GetResponse_T *respArray, size_t respSize, size_t *respLength);
static inline Retcode_T FindQCFGHandler(AtQuectel_QCFG_Setting_T setting, const struct AtQuectel_QCFG_Handler_S **handler);
static inline Retcode_T FindQURCCFGHandler(AtQuectel_QURCCFG_Setting_T setting, const struct AtQuectel_QURCCFG_Handler_S **handler);

static const struct AtQuectel_QINDCFG_Mapping_S QINDCFG_Map[ATQUECTEL_QINDCFG_URCTYPE_MAX] = {
    {ATQUECTEL_QINDCFG_URCTYPE_ALL, ARG_ALL},
    {ATQUECTEL_QINDCFG_URCTYPE_CSQ, ARG_CSQ},
    {ATQUECTEL_QINDCFG_URCTYPE_SMSFULL, ARG_SMSFULL},
    {ATQUECTEL_QINDCFG_URCTYPE_RING, ARG_RING},
    {ATQUECTEL_QINDCFG_URCTYPE_SMSINCOMING, ARG_SMSINCOMING},
};

static const struct AtQuectel_QCFG_Handler_S QCFG_Handlers[QCFG_HANDLERS_MAX] = {ATQUECTEL_QCFG_HANDLER_TABLE(ATQUECTEL_QCFG_HANDLER_INLINEINIT)};
static const struct AtQuectel_QURCCFG_Handler_S QURCCFG_Handlers[QURCCFG_HANDLERS_MAX] = {
    ATQUECTEL_QURCCFG_HANDLER_TABLE(ATQUECTEL_QURCCFG_HANDLER_INLINEINIT)};

static Retcode_T HandleCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E expectedCode)
{
    enum AtTransceiver_ResponseCode_E code;
    Retcode_T rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);

    if (RETCODE_OK == rc && code != expectedCode)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

static inline Retcode_T HandleCodeOk(struct AtTransceiver_S *t)
{
    return HandleCode(t, ATTRANSCEIVER_RESPONSECODE_OK);
}

static Retcode_T FlushAndHandleCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E expectedCode)
{
    Retcode_T rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        rc = HandleCode(t, expectedCode);
    }

    return rc;
}

static inline Retcode_T FlushAndHandleCodeOk(struct AtTransceiver_S *t)
{
    return FlushAndHandleCode(t, ATTRANSCEIVER_RESPONSECODE_OK);
}

static Retcode_T WriteNwScanModeConfig(struct AtTransceiver_S *t, const AtQuectel_QCFG_Set_T *set)
{
    Retcode_T rc = AtTransceiver_WriteI32(t, (int32_t)set->Value.NwScanMode.ScanMode, ATTRANSCEIVER_DECIMAL);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Value.NwScanMode.TakeEffectImmediately, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

static Retcode_T ReadNwScanModeConfig(struct AtTransceiver_S *t, AtQuectel_QCFG_QueryResponse_T *resp)
{
    Retcode_T rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QCFG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, SHORT_TIMEOUT);
        resp->Setting = ATQUECTEL_QCFG_SETTING_NWSCANMODE;
    }

    if (RETCODE_OK == rc)
    {
        int32_t scanmode = 0;
        rc = AtTransceiver_ReadI32(t, &scanmode, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Value.NwScanMode.ScanMode = (AtQuectel_QCFG_NwScanMode_ScanMode_T)scanmode;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

static Retcode_T WriteNwScanSeqConfig(struct AtTransceiver_S *t, const AtQuectel_QCFG_Set_T *set)
{
    Retcode_T rc = AtTransceiver_Write(t, ARG_SEPARATOR, strlen(ARG_SEPARATOR), ATTRANSCEIVER_WRITESTATE_ARGUMENT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, set->Value.NwScanSeq.ScanSeq, strlen(set->Value.NwScanSeq.ScanSeq), ATTRANSCEIVER_WRITESTATE_ARGUMENT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Value.NwScanSeq.TakeEffectImmediately, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

static Retcode_T ReadNwScanSeqConfig(struct AtTransceiver_S *t, AtQuectel_QCFG_QueryResponse_T *resp)
{
    Retcode_T rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QCFG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, SHORT_TIMEOUT);
        resp->Setting = ATQUECTEL_QCFG_SETTING_NWSCANSEQ;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadArgument(t, resp->Value.NwScanSeq.ScanSeq, sizeof(resp->Value.NwScanSeq.ScanSeq), SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

static Retcode_T WriteIoTOpModeConfig(struct AtTransceiver_S *t, const AtQuectel_QCFG_Set_T *set)
{
    Retcode_T rc = AtTransceiver_WriteI32(t, (int32_t)set->Value.IoTOpMode.Mode, ATTRANSCEIVER_DECIMAL);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Value.IoTOpMode.TakeEffectImmediately, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

static Retcode_T ReadIoTOpModeConfig(struct AtTransceiver_S *t, AtQuectel_QCFG_QueryResponse_T *resp)
{
    Retcode_T rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QCFG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, SHORT_TIMEOUT);
        resp->Setting = ATQUECTEL_QCFG_SETTING_IOTOPMODE;
    }

    if (RETCODE_OK == rc)
    {
        int32_t iotmode = 0;
        rc = AtTransceiver_ReadI32(t, &iotmode, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Value.IoTOpMode.Mode = (AtQuectel_QCFG_IoTOpMode_Mode_T)iotmode;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

static Retcode_T WriteUrcPortConfig(struct AtTransceiver_S *t, const AtQuectel_QURCCFG_Set_T *set)
{
    Retcode_T rc = RETCODE_OK;

    switch (set->Value.UrcPort.UrcPortValue)
    {
    case ATQUECTEL_QURCCFG_URCPORTVALUE_USBAT:
        rc = AtTransceiver_WriteString(t, ARG_USBAT);
        break;
    case ATQUECTEL_QURCCFG_URCPORTVALUE_USBMODEM:
        rc = AtTransceiver_WriteString(t, ARG_USBMODEM);
        break;
    case ATQUECTEL_QURCCFG_URCPORTVALUE_UART1:
        rc = AtTransceiver_WriteString(t, ARG_UART1);
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

static Retcode_T ReadUrcPortConfig(struct AtTransceiver_S *t, AtQuectel_QURCCFG_QueryResponse_T *resp)
{
    Retcode_T rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QURCCFG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, SHORT_TIMEOUT);
        resp->Setting = ATQUECTEL_QURCCFG_SETTING_URCPORT;
    }

    char stringBuf[ATQUECTEL_MAX3(sizeof(ARG_USBAT), sizeof(ARG_USBMODEM), sizeof(ARG_UART1)) + 1];
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        if (0U == strncmp(stringBuf, ARG_USBAT, sizeof(stringBuf)))
        {
            resp->Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_USBAT;
        }
        else if (0U == strncmp(stringBuf, ARG_USBMODEM, sizeof(stringBuf)))
        {
            resp->Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_USBMODEM;
        }
        else if (0U == strncmp(stringBuf, ARG_UART1, sizeof(stringBuf)))
        {
            resp->Value.UrcPort.UrcPortValue = ATQUECTEL_QURCCFG_URCPORTVALUE_UART1;
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

static uint8_t FromHexChar(char hexChar)
{
    uint8_t result;

    if ('0' <= hexChar && hexChar <= '9')
    {
        result = hexChar - '0';
    }
    else if ('A' <= hexChar && hexChar <= 'F')
    {
        result = 10 + (hexChar - 'A');
    }
    else if ('a' <= hexChar && hexChar <= 'f')
    {
        result = 10 + (hexChar - 'a');
    }
    else
    {
        result = UINT8_MAX;
    }

    return result;
}

static Retcode_T ParseIPv6RightToLeft(const char *addressBuff, size_t addressBuffLen,
                                      AtQuectel_Address_T *parsedAddress, size_t alreadyParsedGroups)
{
    Retcode_T retcode = RETCODE_OK;
    uint32_t currentGroup = 0;
    uint8_t positionInCurrentGroup = 0;
    uint8_t groupsParsed = 0;
    int32_t prevColon = 0;

    for (int32_t i = addressBuffLen - 1; i >= 0; i--)
    {
        char c = (char)toupper(addressBuff[i]);
        uint8_t numVal = FromHexChar(addressBuff[i]);
        if (':' == c)
        {
            if (i != 0)
            {
                if (prevColon == i + 1 || ATQUECTEL_IPV6GROUPCOUNT <= alreadyParsedGroups)
                {
                    /* We found another skip segment? Or found one segment too
                     * many? IPv6 is malformed! */
                    retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
                    break;
                }
            }
            prevColon = i;
            parsedAddress->Address.IPv6[groupsParsed] = currentGroup;
            groupsParsed++;
            alreadyParsedGroups++;

            positionInCurrentGroup = 0;
            currentGroup = 0;
        }
        else if (UINT8_MAX != numVal)
        {
            /* We found a hex number, add it to our current group. */
            uint32_t pow = 1;
            for (uint32_t j = 0; j < positionInCurrentGroup; ++j)
            {
                pow *= 16;
            }
            positionInCurrentGroup++;
            currentGroup += numVal * pow;

            if (positionInCurrentGroup > 4 || currentGroup > UINT16_MAX)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            }
        }
        else
        {
            /* Unrecognizable char in byte buffer! */
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            break;
        }

        if (i == 0 && ':' != c)
        {
            parsedAddress->Address.IPv6[groupsParsed] = currentGroup;
            groupsParsed++;
        }
    }

    return retcode;
}

static Retcode_T ParseIPv6LeftToRight(const char *addressBuff, size_t addressBuffLen,
                                      AtQuectel_Address_T *parsedAddress)
{
    Retcode_T retcode = RETCODE_OK;
    uint32_t currentGroup = 0;
    uint8_t positionInCurrentGroup = 0;
    uint8_t groupsParsed = 0;
    uint32_t lastColon = 0;

    for (uint32_t i = 0; i < addressBuffLen; i++)
    {
        char c = (char)toupper(addressBuff[i]);
        uint8_t numVal = FromHexChar(addressBuff[i]);
        if (':' == c)
        {
            groupsParsed++;
            if (lastColon == i - 1)
            {
                /* Found a zero segment skip, switch to parsing from right
                 * to left now */
                return ParseIPv6RightToLeft(addressBuff + i, addressBuffLen - i, parsedAddress, groupsParsed);
            }
            else if (ATQUECTEL_IPV6GROUPCOUNT <= groupsParsed)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            }
            else
            {
                lastColon = i;
                parsedAddress->Address.IPv6[ATQUECTEL_IPV6GROUPCOUNT - groupsParsed] = currentGroup;

                positionInCurrentGroup = 0;
                currentGroup = 0;
            }
        }
        else if (UINT8_MAX != numVal)
        {
            /* We found a hex number, add it to our current group. */
            currentGroup *= 16;
            currentGroup += numVal;

            positionInCurrentGroup++;

            if (positionInCurrentGroup > 4 || currentGroup > UINT16_MAX)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            }
        }
        else
        {
            /* Unrecognizable char in byte buffer! */
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            break;
        }

        if (RETCODE_OK == retcode && i == addressBuffLen - 1)
        {
            groupsParsed++;
            parsedAddress->Address.IPv6[ATQUECTEL_IPV6GROUPCOUNT - groupsParsed] = currentGroup;
        }
    }

    return retcode;
}

static Retcode_T ParseIPv4(const char *addressBuff, size_t addressBuffLen,
                           AtQuectel_Address_T *parsedAddress)
{
    Retcode_T retcode = RETCODE_OK;
    uint32_t currentGroup = 0;
    uint8_t positionInCurrentGroup = 0;
    uint8_t groupsParsed = 0;

    for (size_t i = 0; i < addressBuffLen; i++)
    {
        char c = (char)toupper(addressBuff[i]);
        if ('.' == c)
        {
            groupsParsed++;
            if (ATQUECTEL_IPV4GROUPCOUNT <= groupsParsed)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            }
            else
            {
                parsedAddress->Address.IPv4[ATQUECTEL_IPV4GROUPCOUNT - groupsParsed] = currentGroup;

                positionInCurrentGroup = 0;
                currentGroup = 0;
            }
        }
        else if (isdigit((int)c))
        {
            uint8_t numVal = addressBuff[i] - '0';
            /* We found a hex number, add it to our current group. */
            currentGroup *= 10;
            currentGroup += numVal;

            positionInCurrentGroup++;

            if (positionInCurrentGroup > 3 || currentGroup > UINT8_MAX)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            }
        }
        else
        {
            /* Unrecognizable char in byte buffer! */
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            break;
        }

        if (RETCODE_OK == retcode && i == addressBuffLen - 1)
        {
            groupsParsed++;
            parsedAddress->Address.IPv4[ATQUECTEL_IPV4GROUPCOUNT - groupsParsed] = currentGroup;
        }
    }

    return retcode;
}

static Retcode_T ParseQuectelAddress(const char *addressBuff, size_t addressBuffLen,
                                     AtQuectel_Address_T *parsedAddress)
{
    if (NULL == addressBuff || NULL == parsedAddress || 0 == addressBuffLen || addressBuffLen > ATQUECTEL_MAXIPSTRLENGTH)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    Retcode_T retcode = RETCODE_OK;

    /* First figure out if this is a IPv4 or IPv6 address. */
    parsedAddress->Type = ATQUECTEL_ADDRESSTYPE_INVALID;
    for (uint32_t i = 0; i < addressBuffLen; i++)
    {
        if ('.' == addressBuff[i])
        {
            parsedAddress->Type = ATQUECTEL_ADDRESSTYPE_IPV4;
            break;
        }
        else if (':' == addressBuff[i])
        {
            parsedAddress->Type = ATQUECTEL_ADDRESSTYPE_IPV6;
            break;
        }
    }

    switch (parsedAddress->Type)
    {
    case ATQUECTEL_ADDRESSTYPE_IPV4:
        memset(parsedAddress->Address.IPv4, 0, sizeof(parsedAddress->Address.IPv4));
        retcode = ParseIPv4(addressBuff, addressBuffLen, parsedAddress);
        break;
    case ATQUECTEL_ADDRESSTYPE_IPV6:
        memset(parsedAddress->Address.IPv6, 0, sizeof(parsedAddress->Address.IPv6));
        retcode = ParseIPv6LeftToRight(addressBuff, addressBuffLen, parsedAddress);
        break;
    case ATQUECTEL_ADDRESSTYPE_INVALID:
    default:
        memset(parsedAddress->Address.IPv6, 0, sizeof(parsedAddress->Address.IPv6));
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
        break;
    }

    return retcode;
}

static Retcode_T WriteQuectelAddress(struct AtTransceiver_S *t, const AtQuectel_Address_T *addr)
{
    char ipBuffer[ATQUECTEL_MAXIPSTRLENGTH + 1];

    switch (addr->Type)
    {
    case ATQUECTEL_ADDRESSTYPE_IPV4:
        snprintf(ipBuffer, sizeof(ipBuffer), "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
                 addr->Address.IPv4[3],
                 addr->Address.IPv4[2],
                 addr->Address.IPv4[1],
                 addr->Address.IPv4[0]);
        break;
    case ATQUECTEL_ADDRESSTYPE_IPV6:
        snprintf(ipBuffer, sizeof(ipBuffer), "%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16 ":%" PRIx16,
                 addr->Address.IPv6[7],
                 addr->Address.IPv6[6],
                 addr->Address.IPv6[5],
                 addr->Address.IPv6[4],
                 addr->Address.IPv6[3],
                 addr->Address.IPv6[2],
                 addr->Address.IPv6[1],
                 addr->Address.IPv6[0]);
        break;
    default:
        /* Should've been caught by param check at the begining. */
        assert(0);
        break;
    }
    return AtTransceiver_WriteString(t, ipBuffer);
}

static Retcode_T ParseResponsesFromQISTATE(struct AtTransceiver_S *t, AtQuectel_QISTATE_GetResponse_T *respArray, size_t respSize, size_t *respLength)
{
    bool hasNoResponse = true;
    Retcode_T rc = AtTransceiver_CheckEndOfLine(t, &hasNoResponse, SHORT_TIMEOUT);

    char stringBuf[ATQUECTEL_MAX9(
                       sizeof(ARG_TCP),
                       sizeof(ARG_UDP),
                       sizeof(ARG_TCPLISTENER),
                       sizeof(ARG_TCPINCOMING),
                       sizeof(ARG_UDPSERVICE),
                       sizeof(ARG_USBAT),
                       sizeof(ARG_USBMODEM),
                       sizeof(ARG_UART1),
                       ATQUECTEL_MAXIPSTRLENGTH) +
                   1];
    for (size_t i = 0; !hasNoResponse && i < respSize && RETCODE_OK == rc; i++)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QISTATE, SHORT_TIMEOUT);

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadU32(t, &respArray[i].ConnectId, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            if (0 == strncmp(stringBuf, ARG_TCP, sizeof(stringBuf)))
            {
                respArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCP;
            }
            else if (0 == strncmp(stringBuf, ARG_UDP, sizeof(stringBuf)))
            {
                respArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_UDP;
            }
            else if (0 == strncmp(stringBuf, ARG_TCPLISTENER, sizeof(stringBuf)))
            {
                respArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPLISTENER;
            }
            else if (0 == strncmp(stringBuf, ARG_TCPINCOMING, sizeof(stringBuf)))
            {
                respArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_TCPINCOMING;
            }
            else if (0 == strncmp(stringBuf, ARG_UDPSERVICE, sizeof(stringBuf)))
            {
                respArray[i].ServiceType = ATQUECTEL_QISTATE_SERVICETYPE_UDPSERVICE;
            }
            else
            {
                rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            }
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            rc = ParseQuectelAddress(stringBuf, strlen(stringBuf), &respArray[i].IpAddress);
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadU16(t, &respArray[i].RemotePort, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadU16(t, &respArray[i].LocalPort, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            int32_t tmp = 0;
            rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            respArray[i].SocketState = (AtQuectel_QISTATE_SocketState_T)tmp;
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadU32(t, &respArray[i].ContextId, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadU32(t, &respArray[i].ServerId, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            int32_t tmp = 0;
            rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            respArray[i].AccessMode = (AtQuectel_DataAccessMode_T)tmp;
        }

        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            if (0 == strncmp(stringBuf, ARG_USBAT, sizeof(stringBuf)))
            {
                respArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_USBAT;
            }
            else if (0 == strncmp(stringBuf, ARG_USBMODEM, sizeof(stringBuf)))
            {
                respArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_USBMODEM;
            }
            else if (0 == strncmp(stringBuf, ARG_UART1, sizeof(stringBuf)))
            {
                respArray[i].AtPort = ATQUECTEL_QISTATE_ATPORT_UART1;
            }
            else
            {
                rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            }
        }

        if (RETCODE_OK == rc)
        {
            if (respLength)
                (*respLength)++;
            rc = AtTransceiver_CheckEndOfLine(t, &hasNoResponse, SHORT_TIMEOUT);
        }
    }

    return rc;
}

static inline Retcode_T FindQCFGHandler(AtQuectel_QCFG_Setting_T setting, const struct AtQuectel_QCFG_Handler_S **handler)
{
    Retcode_T rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);

    for (size_t i = 0; i < QCFG_HANDLERS_MAX && RETCODE_OK != rc; ++i)
    {
        if (QCFG_Handlers[i].Setting == setting)
        {
            *handler = &QCFG_Handlers[i];
            rc = RETCODE_OK;
        }
    }

    return rc;
}

static inline Retcode_T FindQURCCFGHandler(AtQuectel_QURCCFG_Setting_T setting, const struct AtQuectel_QURCCFG_Handler_S **handler)
{
    Retcode_T rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);

    for (size_t i = 0; i < QURCCFG_HANDLERS_MAX && RETCODE_OK != rc; ++i)
    {
        if (QURCCFG_Handlers[i].Setting == setting)
        {
            *handler = &QURCCFG_Handlers[i];
            rc = RETCODE_OK;
        }
    }

    return rc;
}

Retcode_T AtQuectel_QueryQCFG(struct AtTransceiver_S *t, const AtQuectel_QCFG_Query_T *query, AtQuectel_QCFG_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QCFG_Handler_S *handler;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = FindQCFGHandler(query->Setting, &handler);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(NULL != handler->String);
        rc = AtTransceiver_WriteString(t, handler->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = handler->HandleConfigRead(t, resp);
    }

    return rc;
}

Retcode_T AtQuectel_SetQCFG(struct AtTransceiver_S *t, const AtQuectel_QCFG_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QCFG_Handler_S *handler;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = FindQCFGHandler(set->Setting, &handler);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(NULL != handler->String);
        rc = AtTransceiver_WriteString(t, handler->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = handler->HandleConfigWrite(t, set);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQURCCFG(struct AtTransceiver_S *t, const AtQuectel_QURCCFG_Query_T *query, AtQuectel_QURCCFG_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QURCCFG_Handler_S *handler;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = FindQURCCFGHandler(query->Setting, &handler);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QURCCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(NULL != handler->String);
        rc = AtTransceiver_WriteString(t, handler->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = handler->HandleConfigRead(t, resp);
    }

    return rc;
}

Retcode_T AtQuectel_SetQURCCFG(struct AtTransceiver_S *t, const AtQuectel_QURCCFG_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QURCCFG_Handler_S *handler;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = FindQURCCFGHandler(set->Setting, &handler);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QURCCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(NULL != handler->String);
        rc = AtTransceiver_WriteString(t, handler->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = handler->HandleConfigWrite(t, set);
    }

    return rc;
}

Retcode_T AtQuectel_ExecuteQCCID(struct AtTransceiver_S *t, AtQuectel_QCCID_ExecuteResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = AtTransceiver_WriteAction(t, CMD_SEPARATOR CMD_QCCID);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QCCID, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadArgument(t, resp->Iccid, sizeof(resp->Iccid), SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQINDCFG(struct AtTransceiver_S *t, const AtQuectel_QINDCFG_Query_T *query, AtQuectel_QINDCFG_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QINDCFG_Mapping_S *mapping;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QINDCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(query->UrcType < ATQUECTEL_QINDCFG_URCTYPE_MAX);
        mapping = &QINDCFG_Map[query->UrcType];
        assert(mapping->Enum == urcType);

        rc = AtTransceiver_WriteString(t, mapping->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QINDCFG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, SHORT_TIMEOUT); // skip the <urctype>
        resp->UrcType = query->UrcType;
    }

    if (RETCODE_OK == rc)
    {
        int32_t tmp = 0;
        rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Enable = (bool)tmp;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_SetQINDCFG(struct AtTransceiver_S *t, const AtQuectel_QINDCFG_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;
    const struct AtQuectel_QINDCFG_Mapping_S *mapping;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QINDCFG);
    }

    if (RETCODE_OK == rc)
    {
        assert(set->UrcType < ATQUECTEL_QINDCFG_URCTYPE_MAX);
        mapping = &QINDCFG_Map[set->UrcType];
        assert(mapping->Enum == urcType);

        rc = AtTransceiver_WriteString(t, mapping->String);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Enable, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->SaveToNonVolatileRam, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_ExecuteQINISTAT(struct AtTransceiver_S *t, AtQuectel_QINISTAT_ExecuteResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteAction(t, CMD_SEPARATOR CMD_QINISTAT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QINISTAT, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        int32_t tmp = 0;
        rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Status = (AtQuectel_QINISTAT_Status_T)tmp;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQICSGP(struct AtTransceiver_S *t, const AtQuectel_QICSGP_Query_T *query, AtQuectel_QICSGP_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QICSGP);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, query->ContextId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QICSGP, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        int32_t tmp = 0;
        rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->ContextType = (AtQUectel_QICSGP_ContextType_T)tmp;
    }

    if (RETCODE_OK == rc && resp->Apn && resp->ApnSize > 0)
    {
        rc = AtTransceiver_ReadString(t, resp->Apn, resp->ApnSize, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && resp->Username && resp->UsernameSize > 0)
    {
        rc = AtTransceiver_ReadString(t, resp->Username, resp->UsernameSize, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && resp->Password && resp->PasswordSize > 0)
    {
        rc = AtTransceiver_ReadString(t, resp->Password, resp->PasswordSize, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        int32_t tmp = 0;
        rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Authentication = (AtQuectel_QICSGP_Authentication_T)tmp;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_SetQICSGP(struct AtTransceiver_S *t, const AtQuectel_QICSGP_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QICSGP);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ContextId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, set->ContextType, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteString(t, set->Apn);
    }

    if (RETCODE_OK == rc && ATQUECTEL_QICSGP_AUTHENTICATION_NONE != set->Authentication)
    {
        rc = AtTransceiver_WriteString(t, set->Username);
    }

    if (RETCODE_OK == rc && ATQUECTEL_QICSGP_AUTHENTICATION_NONE != set->Authentication)
    {
        rc = AtTransceiver_WriteString(t, set->Password);
    }

    if (RETCODE_OK == rc && ATQUECTEL_QICSGP_AUTHENTICATION_NONE != set->Authentication)
    {
        rc = AtTransceiver_WriteI32(t, set->Authentication, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_SetQIACT(struct AtTransceiver_S *t, const AtQuectel_QIACT_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QIACT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ContextId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_GetQIACT(struct AtTransceiver_S *t, AtQuectel_QIACT_GetResponse_T *respArray, size_t respSize, size_t *respLength)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == respArray && respSize > 0)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_QIACT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        if (respLength)
            *respLength = 0;
    }

    bool lastResponse = false;
    for (size_t i = 0; RETCODE_OK == rc && i < respSize && !lastResponse; ++i)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QIACT, SHORT_TIMEOUT);

        if (RETCODE_OK == rc)
        {
            uint32_t tmp = 0;
            rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            respArray[i].ContextId = tmp;
        }

        if (RETCODE_OK == rc)
        {
            int32_t tmp = 0;
            rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            respArray[i].ContextState = (bool)tmp;
        }

        if (RETCODE_OK == rc)
        {
            int32_t tmp = 0;
            rc = AtTransceiver_ReadI32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            respArray[i].ContextType = (AtQuectel_QIACT_ContextType_T)tmp;
        }

        char stringBuf[ATQUECTEL_MAXIPSTRLENGTH + 1];
        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);
        }

        if (RETCODE_OK == rc)
        {
            rc = ParseQuectelAddress(stringBuf, strlen(stringBuf), &respArray[i].IpAddress);
        }

        if (RETCODE_OK == rc)
        {
            if (respLength)
                (*respLength)++;

            rc = AtTransceiver_CheckEndOfLine(t, &lastResponse, SHORT_TIMEOUT);
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_SetQIDEACT(struct AtTransceiver_S *t, const AtQuectel_QIDEACT_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QIDEACT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ContextId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_SetQIOPEN(struct AtTransceiver_S *t, const AtQuectel_QIOPEN_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set ||
        set->ServiceType < ATQUECTEL_QIOPEN_SERVICETYPE_TCP ||
        set->ServiceType > ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE ||
        set->RemoteEndpoint.Type < ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS ||
        set->RemoteEndpoint.Type > ATQUECTEL_QIOPEN_ENDPOINTTYPE_DOMAINNAME ||
        (set->RemoteEndpoint.Type == ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS &&
         (set->RemoteEndpoint.Endpoint.IpAddress.Type < ATQUECTEL_ADDRESSTYPE_IPV4 ||
          set->RemoteEndpoint.Endpoint.IpAddress.Type > ATQUECTEL_ADDRESSTYPE_IPV6)) ||
        set->AccessMode < ATQUECTEL_DATAACCESSMODE_BUFFER ||
        set->AccessMode > ATQUECTEL_DATAACCESSMODE_TRANSPARENT)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QIOPEN);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ContextId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ConnectId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        switch (set->ServiceType)
        {
        case ATQUECTEL_QIOPEN_SERVICETYPE_TCP:
            rc = AtTransceiver_WriteString(t, ARG_TCP);
            break;
        case ATQUECTEL_QIOPEN_SERVICETYPE_UDP:
            rc = AtTransceiver_WriteString(t, ARG_UDP);
            break;
        case ATQUECTEL_QIOPEN_SERVICETYPE_TCPLISTENER:
            rc = AtTransceiver_WriteString(t, ARG_TCPLISTENER);
            break;
        case ATQUECTEL_QIOPEN_SERVICETYPE_UDPSERVICE:
            rc = AtTransceiver_WriteString(t, ARG_UDPSERVICE);
            break;
        default:
            /* Should've been caught by param check at the begining. */
            assert(0);
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        switch (set->RemoteEndpoint.Type)
        {
        case ATQUECTEL_QIOPEN_ENDPOINTTYPE_IPADDRESS:
            rc = WriteQuectelAddress(t, &set->RemoteEndpoint.Endpoint.IpAddress);
            break;
        case ATQUECTEL_QIOPEN_ENDPOINTTYPE_DOMAINNAME:
            rc = AtTransceiver_WriteString(t, set->RemoteEndpoint.Endpoint.DomainName);
            break;
        default:
            /* Should've been caught by param check at the begining. */
            assert(0);
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU16(t, set->RemotePort, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU16(t, set->LocalPort, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, set->AccessMode, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        switch (set->AccessMode)
        {
        case ATQUECTEL_DATAACCESSMODE_BUFFER:
        case ATQUECTEL_DATAACCESSMODE_DIRECT:
            rc = FlushAndHandleCodeOk(t);
            break;
        case ATQUECTEL_DATAACCESSMODE_TRANSPARENT:
            rc = FlushAndHandleCode(t, ATTRANSCEIVER_RESPONSECODE_CONNECT);
            break;
        default:
            /* Should've been caught by param check at the begining. */
            assert(0);
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
            break;
        }
    }

    return rc;
}

Retcode_T AtQuectel_SetQICLOSE(struct AtTransceiver_S *t, const AtQuectel_QICLOSE_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QICLOSE);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ConnectId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU16(t, set->Timeout, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = FlushAndHandleCodeOk(t);
    }

    return rc;
}

Retcode_T AtQuectel_GetQISTATE(struct AtTransceiver_S *t, AtQuectel_QISTATE_GetResponse_T *respArray, size_t respSize, size_t *respLength)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == respArray || respSize <= 0)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_QISTATE);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ParseResponsesFromQISTATE(t, respArray, respSize, respLength);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQISTATE(struct AtTransceiver_S *t, const AtQuectel_QISTATE_Query_T *query, AtQuectel_QISTATE_QueryResponse_T *respArray, size_t respSize, size_t *respLength)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == respArray ||
        respSize <= 0 ||
        query->QueryType < ATQUECTEL_QISTATE_QUERYTYPE_CONTEXTID ||
        query->QueryType > ATQUECTEL_QISTATE_QUERYTYPE_CONNECTID)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QISTATE);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, query->QueryType, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        switch (query->QueryType)
        {
        case ATQUECTEL_QISTATE_QUERYTYPE_CONNECTID:
            rc = AtTransceiver_WriteI32(t, query->Query.ConnectId, ATTRANSCEIVER_DECIMAL);
            break;
        case ATQUECTEL_QISTATE_QUERYTYPE_CONTEXTID:
            rc = AtTransceiver_WriteI32(t, query->Query.ConnectId, ATTRANSCEIVER_DECIMAL);
            break;
        default:
        case ATQUECTEL_QISTATE_QUERYTYPE_INVALID:
            /* Should've been caught by param check at the begining. */
            assert(0);
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ParseResponsesFromQISTATE(t, respArray, respSize, respLength);
    }

    return rc;
}

Retcode_T AtQuectel_SetQISEND(struct AtTransceiver_S *t, const AtQuectel_QISEND_Set_T *set)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == set ||
        (set->RemoteIp.Type != ATQUECTEL_ADDRESSTYPE_INVALID &&
         (set->RemoteIp.Type < ATQUECTEL_ADDRESSTYPE_IPV4 ||
          set->RemoteIp.Type > ATQUECTEL_ADDRESSTYPE_IPV6)))
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QISEND);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->ConnectId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, set->Length, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        switch (set->RemoteIp.Type)
        {
        case ATQUECTEL_ADDRESSTYPE_IPV4:
        case ATQUECTEL_ADDRESSTYPE_IPV6:
            rc = WriteQuectelAddress(t, &set->RemoteIp);

            if (RETCODE_OK == rc)
            {
                rc = AtTransceiver_WriteU16(t, set->RemotePort, ATTRANSCEIVER_DECIMAL);
            }
            break;
        case ATQUECTEL_ADDRESSTYPE_INVALID:
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNEXPECTED_BEHAVIOR);
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    bool promptFound = false;
    while (RETCODE_OK == rc && !promptFound)
    {
        char c = '\0';
        rc = AtTransceiver_Read(t, &c, sizeof(c), NULL, SHORT_TIMEOUT);
        if (RETCODE_OK == rc && c == '>')
        {
            promptFound = true;
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, set->Payload, set->Length, ATTRANSCEIVER_WRITESTATE_END);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipBytes(t, set->Length, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCode(t, ATTRANSCEIVER_RESPONSECODE_SENDOK);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQISEND(struct AtTransceiver_S *t, const AtQuectel_QISEND_Query_T *query, AtQuectel_QISEND_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QISEND);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, query->ConnectId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, 0, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QISEND, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        uint32_t tmp = 0;
        rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        assert(tmp > SIZE_MAX);
        resp->TotalSendLength = (size_t)tmp;
    }

    if (RETCODE_OK == rc)
    {
        uint32_t tmp = 0;
        rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        assert(tmp > SIZE_MAX);
        resp->AcknowledgedBytes = (size_t)tmp;
    }

    if (RETCODE_OK == rc)
    {
        uint32_t tmp = 0;
        rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        assert(tmp > SIZE_MAX);
        resp->UnacknowledgedBytes = (size_t)tmp;
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

static Retcode_T ParseStatisticsFromQIRD(struct AtTransceiver_S *t, AtQuectel_QIRD_QueryResponse_T *resp)
{
    uint32_t tmp = 0;
    Retcode_T rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    assert(tmp > SIZE_MAX);
    resp->Statistics.TotalReceiveLength = (size_t)tmp;

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        assert(tmp > SIZE_MAX);
        resp->Statistics.HaveReadLength = (size_t)tmp;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        assert(tmp > SIZE_MAX);
        resp->Statistics.UnreadLength = (size_t)tmp;
    }

    return rc;
}

static Retcode_T ParsePayloadFromQIRD(struct AtTransceiver_S *t, const AtQuectel_QIRD_Query_T *query, AtQuectel_QIRD_QueryResponse_T *resp)
{
    uint32_t tmp = 0;
    Retcode_T rc = AtTransceiver_ReadU32(t, &tmp, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    assert(tmp > SIZE_MAX);
    resp->Payload.ReadActualLength = (size_t)tmp;
    bool bufferInsufficient = query->ReadLength < resp->Payload.ReadActualLength;

    if (RETCODE_OK == rc)
    {
        if (!AtTransceiver_IsStartOfLine(t))
        {
            char stringBuf[ATQUECTEL_MAXIPSTRLENGTH + 1];
            rc = AtTransceiver_ReadString(t, stringBuf, sizeof(stringBuf), SHORT_TIMEOUT);

            if (RETCODE_OK == rc)
            {
                rc = ParseQuectelAddress(stringBuf, strlen(stringBuf), &resp->Payload.RemoteIp);
            }

            if (RETCODE_OK == rc)
            {
                rc = AtTransceiver_ReadU16(t, &resp->Payload.RemotePort, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            }
        }
        else
        {
            resp->Payload.RemoteIp.Type = ATQUECTEL_ADDRESSTYPE_INVALID;
            resp->Payload.RemotePort = 0;
        }
    }

    if (RETCODE_OK == rc)
    {
        if (NULL != resp->Payload.Data)
        {
            if (bufferInsufficient)
            {
                rc = AtTransceiver_Read(t, resp->Payload.Data, query->ReadLength, NULL, SHORT_TIMEOUT);
            }
            else
            {
                rc = AtTransceiver_Read(t, resp->Payload.Data, resp->Payload.ReadActualLength, NULL, SHORT_TIMEOUT);
            }
        }
        else
        {
            rc = AtTransceiver_SkipBytes(t, resp->Payload.ReadActualLength, SHORT_TIMEOUT);
        }
    }

    if (RETCODE_OK == rc && bufferInsufficient)
    {
        rc = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES);
    }

    return rc;
}

Retcode_T AtQuectel_QueryQIRD(struct AtTransceiver_S *t, const AtQuectel_QIRD_Query_T *query, AtQuectel_QIRD_QueryResponse_T *resp)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    if (NULL == query || NULL == resp)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_QIRD);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, query->ConnectId, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU32(t, query->ReadLength, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_QIRD, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        if (0U >= query->ReadLength)
        {
            rc = ParseStatisticsFromQIRD(t, resp);
        }
        else
        {
            rc = ParsePayloadFromQIRD(t, query, resp);
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = HandleCodeOk(t);
    }

    return rc;
}

#endif
