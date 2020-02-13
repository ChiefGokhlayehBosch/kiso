/******************************************************************************
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

#include "Kiso_CellularModules.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_AT3GPP27007

#include "Kiso_Cellular.h"
#include "At3Gpp27007.h"

#include "AtTransceiver.h"

#include "Kiso_Logging.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#define AT3GPP27007_STRINGIFY(x) #x
#define AT3GPP27007_TOSTRING(x) AT3GPP27007_STRINGIFY(x)

#define URC_TIMEOUT (UINT32_C(120))     /* msec */
#define SHORT_TIMEOUT (UINT32_C(120))   /* msec */
#define CFUN_TIMEOUT (UINT32_C(180000)) /* msec */
#define LONG_TIMEOUT (UINT32_C(150000)) /* msec */

#define IPV4_BYTE_COUNT (UINT32_C(4))
#define IPV6_BYTE_COUNT (UINT32_C(16))
#define MAX_IP_STR_LENGTH (UINT32_C(63)) /* "255.255.255.255" or "255.255.255.255.255.255.255.255.255.255.255.255.255.255.255.255" */

#define CMD_CREG "CREG"
#define CMD_CGREG "CGREG"
#define CMD_CEREG "CEREG"
#define CMD_COPS "COPS"
#define CMD_CGDCONT "CGDCONT"
#define ARG_CGDCONT_PDPTYPE_IP "IP"
#define ARG_CGDCONT_PDPTYPE_IPV6 "IPV6"
#define ARG_CGDCONT_PDPTYPE_IPV4V6 "IPV4V6"
#define CMD_CGACT "CGACT"
#define CMD_CGPADDR "CGPADDR"
#define CMD_CPIN "CPIN"
#define CMD_AT ""
#define CMD_ATE0 "E0"
#define CMD_ATE1 "E1"
#define CMD_CFUN "CFUN"
#define CMD_CMEE "CMEE"

#define CMD_SEPARATOR "+"

static inline void SwapEndianess(void *data, size_t sizeInBytes)
{
    assert(sizeInBytes > 0);

    char *dataAsCharBuf = (char *)data;
    for (size_t lo = 0, hi = sizeInBytes - 1; hi > lo; lo++, hi--)
    {
        char tmp = dataAsCharBuf[lo];
        dataAsCharBuf[lo] = dataAsCharBuf[hi];
        dataAsCharBuf[hi] = tmp;
    }
}

static inline void KillInvalidBytes(void *data, size_t sizeInBytes, size_t numValidBytes)
{
    uint8_t *dataAsBytes = (uint8_t *)data;
    for (size_t i = 0; i < sizeInBytes - numValidBytes; ++i)
    {
        dataAsBytes[sizeInBytes - i - 1] = 0;
    }
}

static Retcode_T ExtractPdpAddress(const char *addressBuff, size_t addressBuffLen, At3Gpp27007_CGPADDR_Address_T *parsedAddress)
{
    if (NULL == addressBuff || NULL == parsedAddress || 0 == addressBuffLen || addressBuffLen > MAX_IP_STR_LENGTH)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    Retcode_T retcode = RETCODE_OK;
    char result[IPV6_BYTE_COUNT] = {0};

    uint32_t currentByte = 0;
    uint8_t positionInCurrentByte = 0;
    uint8_t bytesParsed = 0;
    for (size_t i = 0; i < addressBuffLen; i++)
    {
        char currentChar = addressBuff[i];

        if ('.' == currentChar)
        {
            result[sizeof(result) - bytesParsed - 1] = currentByte;

            bytesParsed++;
            currentByte = 0;
            positionInCurrentByte = 0;
        }
        else if (isdigit((int)currentChar))
        {
            currentByte *= 10;
            currentByte += (currentChar - '0');
            positionInCurrentByte++;

            if (currentByte > 0xFF || positionInCurrentByte > 3)
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
                break;
            }
        }
        else if ('"' == currentChar && (0 == i || (addressBuffLen - 1) == i))
        {
            /* Ignore quote chars at start and end.
             * e.g. "127.0.0.1" */
        }
        else
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
            break;
        }

        if ((addressBuffLen - 1) == i)
        {
            // end of IP address
            result[sizeof(result) - bytesParsed - 1] = currentByte;
            bytesParsed++;
        }
    }

    switch (bytesParsed)
    {
    case IPV4_BYTE_COUNT:
        parsedAddress->Type = AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV4;
        memcpy(parsedAddress->Address.IPv4, result + (sizeof(result) - bytesParsed), bytesParsed);
        retcode = RETCODE_OK;
        break;
    case IPV6_BYTE_COUNT:
        parsedAddress->Type = AT3GPP27007_CGPADDR_ADDRESSTYPE_IPV6;
        memcpy(parsedAddress->Address.IPv6, result + (sizeof(result) - bytesParsed), bytesParsed);
        retcode = RETCODE_OK;
        break;
    default:
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
        break;
    }

    return retcode;
}

static Retcode_T ParseArgumentsCREG(struct AtTransceiver_S *t, At3Gpp27007_CREG_GetResponse_T *resp)
{
    resp->N = AT3GPP27007_CXREG_N_INVALID;
    resp->Stat = AT3GPP27007_CXREG_STAT_INVALID;
    resp->Lac = AT3GPP27007_INVALID_LAC;
    resp->Ci = AT3GPP27007_INVALID_CI;
    resp->AcT = AT3GPP27007_CXREG_ACT_INVALID;

    Retcode_T rcOpt = RETCODE_OK;
    int32_t n = AT3GPP27007_CXREG_N_INVALID;
    Retcode_T rc = AtTransceiver_ReadI32(t, &n, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    resp->N = (At3Gpp27007_CXREG_N_T)n;

    if (RETCODE_OK == rc)
    {
        int32_t stat = AT3GPP27007_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, &stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Stat = (At3Gpp27007_CXREG_Stat_T)stat;
    }

    if (RETCODE_OK == rc)
    {
        size_t numActualBytesRead = 0;
        switch (resp->N)
        {
        case AT3GPP27007_CXREG_N_DISABLED:
        case AT3GPP27007_CXREG_N_URC:
            /* Done. AT3GPP27007_CXREG_N_DISABLED and AT3GPP27007_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT3GPP27007_CXREG_N_URC_LOC:
            /** \todo: cause_type and reject_cause not supported! */
            numActualBytesRead = 0;
            rcOpt = AtTransceiver_ReadHexString(t, &resp->Lac, sizeof(resp->Lac), &numActualBytesRead, SHORT_TIMEOUT);

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Lac, sizeof(resp->Lac), numActualBytesRead);
                SwapEndianess(&resp->Lac, numActualBytesRead);

                numActualBytesRead = 0;
                rcOpt = AtTransceiver_ReadHexString(t, &resp->Ci, sizeof(resp->Ci), &numActualBytesRead, SHORT_TIMEOUT);
            }

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Ci, sizeof(resp->Ci), numActualBytesRead);
                SwapEndianess(&resp->Ci, numActualBytesRead);

                int32_t act = AT3GPP27007_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, &act, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
                resp->AcT = (At3Gpp27007_CXREG_AcT_T)act;
                /* Some modems don't report AcT every time. Will be ignored. */
                KISO_UNUSED(rcOpt);
            }
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR,
                         RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            break;
        }
    }

    return rc;
}

static Retcode_T ParseArgumentsCGREG(struct AtTransceiver_S *t, At3Gpp27007_CGREG_GetResponse_T *resp)
{
    resp->N = AT3GPP27007_CXREG_N_INVALID;
    resp->Stat = AT3GPP27007_CXREG_STAT_INVALID;
    resp->Lac = AT3GPP27007_INVALID_LAC;
    resp->Ci = AT3GPP27007_INVALID_CI;
    resp->AcT = AT3GPP27007_CXREG_ACT_INVALID;
    resp->Rac = AT3GPP27007_INVALID_RAC;

    Retcode_T rcOpt = RETCODE_OK;
    int32_t n = AT3GPP27007_CXREG_N_INVALID;
    Retcode_T rc = AtTransceiver_ReadI32(t, &n, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    resp->N = (At3Gpp27007_CXREG_N_T)n;

    if (RETCODE_OK == rc)
    {
        int32_t stat = AT3GPP27007_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, &stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Stat = (At3Gpp27007_CXREG_Stat_T)stat;
    }

    if (RETCODE_OK == rc)
    {

        size_t numActualBytesRead = 0;
        switch (resp->N)
        {
        case AT3GPP27007_CXREG_N_DISABLED:
        case AT3GPP27007_CXREG_N_URC:
            /* Done. AT3GPP27007_CXREG_N_DISABLED and AT3GPP27007_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT3GPP27007_CXREG_N_URC_LOC:
            /** \todo: cause_type, reject_cause, Active-Time, Periodic-RAU,
             * GPRS-READY-timer not supported! */
            numActualBytesRead = 0;
            rcOpt = AtTransceiver_ReadHexString(t, &resp->Lac, sizeof(resp->Lac), &numActualBytesRead, SHORT_TIMEOUT);

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Lac, sizeof(resp->Lac), numActualBytesRead);
                SwapEndianess(&resp->Lac, numActualBytesRead);

                numActualBytesRead = 0;
                rcOpt = AtTransceiver_ReadHexString(t, &resp->Ci, sizeof(resp->Ci), &numActualBytesRead, SHORT_TIMEOUT);
            }

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Ci, sizeof(resp->Ci), numActualBytesRead);
                SwapEndianess(&resp->Ci, numActualBytesRead);

                int32_t act = AT3GPP27007_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, &act, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
                resp->AcT = (At3Gpp27007_CXREG_AcT_T)act;
            }

            if (RETCODE_OK == rcOpt)
            {
                numActualBytesRead = 0;
                rcOpt = AtTransceiver_ReadHexString(t, &resp->Rac, sizeof(resp->Rac), &numActualBytesRead, SHORT_TIMEOUT);
                KillInvalidBytes(&resp->Rac, sizeof(resp->Rac), numActualBytesRead);
                SwapEndianess(&resp->Rac, numActualBytesRead);

                KISO_UNUSED(rcOpt);
            }
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR,
                         RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            break;
        }
    }

    return rc;
}

static Retcode_T ParseArgumentsCEREG(struct AtTransceiver_S *t, At3Gpp27007_CEREG_GetResponse_T *resp)
{
    resp->N = AT3GPP27007_CXREG_N_INVALID;
    resp->Stat = AT3GPP27007_CXREG_STAT_INVALID;
    resp->Tac = AT3GPP27007_INVALID_TAC;
    resp->Ci = AT3GPP27007_INVALID_CI;
    resp->AcT = AT3GPP27007_CXREG_ACT_INVALID;

    Retcode_T rcOpt = RETCODE_OK;
    int32_t n = AT3GPP27007_CXREG_N_INVALID;
    Retcode_T rc = AtTransceiver_ReadI32(t, &n, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    resp->N = (At3Gpp27007_CXREG_N_T)n;

    if (RETCODE_OK == rc)
    {
        int32_t stat = AT3GPP27007_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, &stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
        resp->Stat = (At3Gpp27007_CXREG_Stat_T)stat;
    }

    if (RETCODE_OK == rc)
    {
        size_t numActualBytesRead = 0;
        switch (resp->N)
        {
        case AT3GPP27007_CXREG_N_DISABLED:
        case AT3GPP27007_CXREG_N_URC:
            /* Done. AT3GPP27007_CXREG_N_DISABLED and AT3GPP27007_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT3GPP27007_CXREG_N_URC_LOC:
            numActualBytesRead = 0;
            rcOpt = AtTransceiver_ReadHexString(t, &resp->Tac, sizeof(resp->Tac), &numActualBytesRead, SHORT_TIMEOUT);

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Tac, sizeof(resp->Tac), numActualBytesRead);
                SwapEndianess(&resp->Tac, numActualBytesRead);

                numActualBytesRead = 0;
                rcOpt = AtTransceiver_ReadHexString(t, &resp->Ci, sizeof(resp->Ci), &numActualBytesRead, SHORT_TIMEOUT);
            }

            if (RETCODE_OK == rcOpt)
            {
                KillInvalidBytes(&resp->Ci, sizeof(resp->Ci), numActualBytesRead);
                SwapEndianess(&resp->Ci, numActualBytesRead);

                int32_t act = AT3GPP27007_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, &act, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
                resp->AcT = (At3Gpp27007_CXREG_AcT_T)act;
            }
            break;
        case AT3GPP27007_CXREG_N_URC_LOC_CAUSE:
        case AT3GPP27007_CXREG_N_URC_LOC_PSM:
        case AT3GPP27007_CXREG_N_URC_LOC_PSM_CAUSE:
            /** \todo: cause_type, reject_cause, Active-Time, Periodic-TAU not
             * supported! */
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR,
                         RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            break;
        }
    }

    return rc;
}

static Retcode_T SetCXREG(struct AtTransceiver_S *t, const char *cmd, const At3Gpp27007_CXREG_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != cmd);

    Retcode_T rc = AtTransceiver_WriteSet(t, cmd);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->N, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

static Retcode_T WriteOperatorInFormat(struct AtTransceiver_S *t, At3Gpp27007_COPS_Format_T format, const At3Gpp27007_COPS_Oper_T *oper)
{
    Retcode_T rc = RETCODE_OK;

    char stringBuf[sizeof(AT3GPP27007_TOSTRING(UINT16_MAX)) + 1];
    switch (format)
    {
    case AT3GPP27007_COPS_FORMAT_LONG_ALPHANUMERIC:
    case AT3GPP27007_COPS_FORMAT_SHORT_ALPHANUMERIC:
        /* Long and short -- being members of the same union, should have the
         * same start address. */
        assert(oper->LongAlphanumeric == oper->ShortAlphanumeric);
        rc = AtTransceiver_WriteString(t, oper->LongAlphanumeric);
        break;
    case AT3GPP27007_COPS_FORMAT_NUMERIC:
        snprintf(stringBuf, sizeof(stringBuf), "%" PRIu16, oper->Numeric);
        rc = AtTransceiver_WriteString(t, stringBuf);
        break;
    default:
        assert(0); /* Should've been caught earlier.*/
        break;
    }

    return rc;
}

/* *** NETWORK COMMANDS ****************************************************** */

Retcode_T At3Gpp27007_SetCREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set)
{
    return SetCXREG(t, CMD_SEPARATOR CMD_CREG, set);
}

Retcode_T At3Gpp27007_SetCGREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set)
{
    return SetCXREG(t, CMD_SEPARATOR CMD_CGREG, set);
}

Retcode_T At3Gpp27007_SetCEREG(struct AtTransceiver_S *t, const At3Gpp27007_CXREG_Set_T *set)
{
    return SetCXREG(t, CMD_SEPARATOR CMD_CEREG, set);
}

Retcode_T At3Gpp27007_GetCREG(struct AtTransceiver_S *t, At3Gpp27007_CREG_GetResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_CREG);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_CREG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ParseArgumentsCREG(t, resp);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_GetCGREG(struct AtTransceiver_S *t, At3Gpp27007_CGREG_GetResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_CGREG);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_CGREG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ParseArgumentsCGREG(t, resp);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_GetCEREG(struct AtTransceiver_S *t, At3Gpp27007_CEREG_GetResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_CEREG);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_CEREG, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ParseArgumentsCEREG(t, resp);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_SetCOPS(struct AtTransceiver_S *t, const At3Gpp27007_COPS_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    Retcode_T rc = RETCODE_OK;
    bool modeOnly = true;
    bool formatOnly = true;

    switch (set->Mode)
    {
    case AT3GPP27007_COPS_MODE_AUTOMATIC:
    case AT3GPP27007_COPS_MODE_DEREGISTER:
        modeOnly = true;
        formatOnly = true;
        /* continue */
        break;
    case AT3GPP27007_COPS_MODE_SET_FORMAT_ONLY:
        modeOnly = false;
        formatOnly = true;
        /* continue */
        break;
    case AT3GPP27007_COPS_MODE_MANUAL:
    case AT3GPP27007_COPS_MODE_MANUAL_THEN_AUTOMATIC:
        switch (set->Format)
        {
        case AT3GPP27007_COPS_FORMAT_LONG_ALPHANUMERIC:
        case AT3GPP27007_COPS_FORMAT_SHORT_ALPHANUMERIC:
        case AT3GPP27007_COPS_FORMAT_NUMERIC:
            modeOnly = false;
            formatOnly = false;
            /* continue */
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
            break;
        }
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_COPS);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Mode, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && !modeOnly)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Format, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && !formatOnly)
    {
        rc = WriteOperatorInFormat(t, set->Format, &set->Oper);
    }

    if (RETCODE_OK == rc && !formatOnly && AT3GPP27007_COPS_ACT_INVALID != set->AcT)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->AcT, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_SetCGDCONT(struct AtTransceiver_S *t, const At3Gpp27007_CGDCONT_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    Retcode_T rc = RETCODE_OK;
    const char *pdpType = NULL;
    bool clearCtx = false;

    switch (set->PdpType)
    {
    case AT3GPP27007_CGDCONT_PDPTYPE_IP:
        pdpType = ARG_CGDCONT_PDPTYPE_IP;
        break;
    case AT3GPP27007_CGDCONT_PDPTYPE_IPV6:
        pdpType = ARG_CGDCONT_PDPTYPE_IPV6;
        break;
    case AT3GPP27007_CGDCONT_PDPTYPE_IPV4V6:
        pdpType = ARG_CGDCONT_PDPTYPE_IPV4V6;
        break;
    case AT3GPP27007_CGDCONT_PDPTYPE_INVALID:
        /* No <PDP_Type> means clear the context! Setting a APN doesn't make sense. */
        if (NULL != set->Apn)
        {
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        }
        clearCtx = true;
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CGDCONT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, set->Cid, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && !clearCtx)
    {
        rc = AtTransceiver_WriteString(t, pdpType);
    }

    if (RETCODE_OK == rc && !clearCtx && NULL != set->Apn)
    {
        rc = AtTransceiver_WriteString(t, set->Apn);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_SetCGACT(struct AtTransceiver_S *t, const At3Gpp27007_CGACT_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    Retcode_T rc = RETCODE_OK;

    rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CGACT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, set->State, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, set->Cid, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_SetCGPADDR(struct AtTransceiver_S *t, const At3Gpp27007_CGPADDR_Query_T *query, At3Gpp27007_CGPADDR_QueryResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != query);
    assert(NULL != resp);

    Retcode_T rc = RETCODE_OK;
    char addressBuffer[MAX_IP_STR_LENGTH + 1];

    rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CGPADDR);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, query->Cid, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_CGPADDR, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadU8(t, &resp->Cid, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        /* Wait for PDP_addr1 in response. */
        rc = AtTransceiver_ReadString(t, addressBuffer, sizeof(addressBuffer), SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = ExtractPdpAddress(addressBuffer, strlen(addressBuffer), &resp->PdpAddr);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

/* *** AT ERROR MESSAGEING ************************************************** */

Retcode_T At3Gpp27007_SetCMEE(struct AtTransceiver_S *t, const At3Gpp27007_CMEE_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    Retcode_T rc = RETCODE_OK;

    switch (set->N)
    {
    case AT3GPP27007_CMEE_N_DISABLED:
    case AT3GPP27007_CMEE_N_NUMERIC:
    case AT3GPP27007_CMEE_N_VERBOSE:
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CMEE);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->N, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

/* *** SIM COMMANDS ********************************************************* */

Retcode_T At3Gpp27007_SetCPIN(struct AtTransceiver_S *t, const At3Gpp27007_CPIN_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    Retcode_T rc = RETCODE_OK;

    rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CPIN);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteString(t, set->Pin);
    }

    if (RETCODE_OK == rc && NULL != set->NewPin)
    {
        rc = AtTransceiver_WriteString(t, set->NewPin);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

/* *** TE-TA INTERFACE COMMANDS ********************************************** */

Retcode_T At3Gpp27007_ExecuteAT(struct AtTransceiver_S *t)
{
    assert(NULL != t);

    Retcode_T rc = AtTransceiver_WriteAction(t, CMD_AT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_ExecuteATE(struct AtTransceiver_S *t, bool enableEcho)
{
    Retcode_T rc = RETCODE_OK;
    if (enableEcho)
    {
        rc = AtTransceiver_WriteAction(t, CMD_ATE1);
    }
    else
    {
        rc = AtTransceiver_WriteAction(t, CMD_ATE0);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

/* *** POWER CONTROL COMMANDS *********************************************** */

Retcode_T At3Gpp27007_SetCFUN(struct AtTransceiver_S *t, const At3Gpp27007_CFUN_Set_T *set)
{
    assert(NULL != t);
    assert(NULL != set);

    switch (set->Fun)
    {
    case AT3GPP27007_CFUN_FUN_MINIMUM:
    case AT3GPP27007_CFUN_FUN_FULL:
    case AT3GPP27007_CFUN_FUN_DISABLETX:
    case AT3GPP27007_CFUN_FUN_DISABLERX:
    case AT3GPP27007_CFUN_FUN_DISABLERXTX:
    case AT3GPP27007_CFUN_FUN_RESERVEDSTART:
    case AT3GPP27007_CFUN_FUN_RESERVEDEND:
    case AT3GPP27007_CFUN_FUN_PREPARESHUTDOWN:
        /* do nothing */
        break;
    case AT3GPP27007_CFUN_FUN_INVALID:
    default:
        if (AT3GPP27007_CFUN_FUN_RESERVEDSTART > set->Fun || AT3GPP27007_CFUN_FUN_RESERVEDEND < set->Fun)
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        }
        break;
    }

    Retcode_T rc = AtTransceiver_WriteSet(t, CMD_SEPARATOR CMD_CFUN);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Fun, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && set->Rst != AT3GPP27007_CFUN_RST_INVALID)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)set->Rst, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

Retcode_T At3Gpp27007_GetCFUN(struct AtTransceiver_S *t, At3Gpp27007_CFUN_GetResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_SEPARATOR CMD_CFUN);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Flush(t, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCommand(t, CMD_CFUN, CFUN_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        int32_t fun = AT3GPP27007_CFUN_FUN_INVALID;
        rc = AtTransceiver_ReadI32(t, &fun, ATTRANSCEIVER_DECIMAL, CFUN_TIMEOUT);
        resp->Fun = (At3Gpp27007_CFUN_Fun_T)fun;
    }

    enum AtTransceiver_ResponseCode_E code;
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_ReadCode(t, &code, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc && code != ATTRANSCEIVER_RESPONSECODE_OK)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_CELLULAR_RESPONDED_ERROR);
    }

    return rc;
}

/* *** URC HANDLERS ********************************************************** */

Retcode_T At3Gpp27007_UrcCREG(struct AtTransceiver_S *t, At3Gpp27007_CREG_UrcResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    return ParseArgumentsCREG(t, resp);
}

Retcode_T At3Gpp27007_UrcCGREG(struct AtTransceiver_S *t, At3Gpp27007_CGREG_UrcResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    return ParseArgumentsCGREG(t, resp);
}

Retcode_T At3Gpp27007_UrcCEREG(struct AtTransceiver_S *t, At3Gpp27007_CEREG_UrcResponse_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    return ParseArgumentsCEREG(t, resp);
}
