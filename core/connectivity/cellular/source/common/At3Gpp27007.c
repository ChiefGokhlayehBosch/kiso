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

/* Include all headers which are needed by this file in the following order:
 * Own public header
 * Own protected header
 * Own private header
 * System header (if necessary)
 * Other headers
 */
#include "Kiso_CellularModules.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_AT3GPP27007

#include "At3Gpp27007.h"

#include "Engine.h"

#include "Kiso_Logging.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

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
#define CMD_3GPP27007_ATCOPS "COPS"
#define CMD_CGDCONT "CGDCONT"
#define ARG_CGDCONT_PDPTYPE_IPIP "IP"
#define ARG_CGDCONT_PDPTYPE_IPIPV6 "IPV6"
#define ARG_CGDCONT_PDPTYPE_IPIPV4V6 "IPV4V6"
#define CMD_CGACT "CGACT"
#define CMD_CGPADDR "CGPADDR"
#define CMD_3GPP27007_ATCPIN "CPIN"
#define CMD_AT ""
#define CMD_ATE0 "E0"
#define CMD_ATE1 "E1"
#define CMD_CFUN "CFUN"
#define CMD_CMEE "CMEE"

static Retcode_T ExtractPdpAddress(const char *addressBuff, size_t addressBuffLen, AT_CGPADDR_Address_T *parsedAddress)
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
        parsedAddress->Type = AT_CGPADDR_ADDRESSTYPE_IPV4;
        memcpy(parsedAddress->Address.IPv4, result + (sizeof(result) - bytesParsed), bytesParsed);
        retcode = RETCODE_OK;
        break;
    case IPV6_BYTE_COUNT:
        parsedAddress->Type = AT_CGPADDR_ADDRESSTYPE_IPV6;
        memcpy(parsedAddress->Address.IPv6, result + (sizeof(result) - bytesParsed), bytesParsed);
        retcode = RETCODE_OK;
        break;
    default:
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
        break;
    }

    return retcode;
}

/* *** NETWORK COMMANDS ****************************************************** */

static Retcode_T Set_CXREG(struct AtTransceiver_S *t, const char *cmd, AT_CXREG_N_T n)
{
    assert(NULL != t);
    assert(NULL != cmd);

    Retcode_T rc = AtTransceiver_WriteSet(t, cmd);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (uint32_t)n, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Set_CREG(struct AtTransceiver_S *t, AT_CXREG_N_T n)
{
    return Set_CXREG(t, CMD_CREG, n);
}

Retcode_T At_Set_CGREG(struct AtTransceiver_S *t, AT_CXREG_N_T n)
{
    return Set_CXREG(t, CMD_CGREG, n);
}

Retcode_T At_Set_CEREG(struct AtTransceiver_S *t, AT_CXREG_N_T n)
{
    return Set_CXREG(t, CMD_CEREG, n);
}

Retcode_T At_Get_CREG(struct AtTransceiver_S *t, AT_CREG_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_CREG);
    Retcode_T rcOpt = RETCODE_OK;

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
        param->N = AT_CXREG_N_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->N, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        param->Stat = AT_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->Stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        switch (param->N)
        {
        case AT_CXREG_N_DISABLED:
        case AT_CXREG_N_URC:
            /* Done. AT_CXREG_N_DISABLED and AT_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT_CXREG_N_URC_LOC:
        case AT_CXREG_N_URC_LOC_CAUSE:
            /** \todo: cause_type and reject_cause not supported! */
            param->Lac = AT_INVALID_LAC;
            rcOpt = AtTransceiver_ReadHexString(t, &param->Lac, sizeof(param->Lac), NULL, SHORT_TIMEOUT);
            /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */

            if (RETCODE_OK == rcOpt)
            {
                param->Ci = AT_INVALID_CI;
                rcOpt = AtTransceiver_ReadHexString(t, &param->Ci, sizeof(param->Ci), NULL, SHORT_TIMEOUT);
                /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
            }

            if (RETCODE_OK == rcOpt)
            {
                param->AcT = AT_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&param->AcT, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
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

Retcode_T At_Get_CGREG(struct AtTransceiver_S *t, AT_CGREG_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_CGREG);
    Retcode_T rcOpt = RETCODE_OK;

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
        param->N = AT_CXREG_N_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->N, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        param->Stat = AT_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->Stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        switch (param->N)
        {
        case AT_CXREG_N_DISABLED:
        case AT_CXREG_N_URC:
            /* Done. AT_CXREG_N_DISABLED and AT_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT_CXREG_N_URC_LOC:
        case AT_CXREG_N_URC_LOC_CAUSE:
        case AT_CXREG_N_URC_LOC_PSM:
        case AT_CXREG_N_URC_LOC_PSM_CAUSE:
            /** \todo: cause_type, reject_cause, Active-Time, Periodic-RAU,
             * GPRS-READY-timer not supported! */
            param->Lac = AT_INVALID_LAC;
            rcOpt = AtTransceiver_ReadHexString(t, &param->Lac, sizeof(param->Lac), NULL, SHORT_TIMEOUT);
            /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */

            if (RETCODE_OK == rcOpt)
            {
                param->Ci = AT_INVALID_CI;
                rcOpt = AtTransceiver_ReadHexString(t, &param->Ci, sizeof(param->Ci), NULL, SHORT_TIMEOUT);
                /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
            }

            if (RETCODE_OK == rcOpt)
            {
                param->AcT = AT_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&param->AcT, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            }

            if (RETCODE_OK == rcOpt)
            {
                param->Rac = AT_INVALID_RAC;
                rcOpt = AtTransceiver_ReadU8(t, &param->Rac, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            }
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR,
                         RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            break;
        }
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

Retcode_T At_Get_CEREG(struct AtTransceiver_S *t, AT_CEREG_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_CEREG);
    Retcode_T rcOpt = RETCODE_OK;

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
        param->N = AT_CXREG_N_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->N, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        param->Stat = AT_CXREG_STAT_INVALID;
        rc = AtTransceiver_ReadI32(t, (int32_t *)&param->Stat, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        switch (param->N)
        {
        case AT_CXREG_N_DISABLED:
        case AT_CXREG_N_URC:
            /* Done. AT_CXREG_N_DISABLED and AT_CXREG_N_URC both only carry the
             * 'stat' parameter */
            break;
        case AT_CXREG_N_URC_LOC:
        case AT_CXREG_N_URC_LOC_CAUSE:
        case AT_CXREG_N_URC_LOC_PSM:
        case AT_CXREG_N_URC_LOC_PSM_CAUSE:
            /** \todo: cause_type, reject_cause, Active-Time, Periodic-TAU not
             * supported! */
            param->Tac = AT_INVALID_TAC;
            rcOpt = AtTransceiver_ReadHexString(t, &param->Tac, sizeof(param->Tac), NULL, SHORT_TIMEOUT);
            /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */

            if (RETCODE_OK == rcOpt)
            {
                param->Ci = AT_INVALID_CI;
                rcOpt = AtTransceiver_ReadHexString(t, &param->Ci, sizeof(param->Ci), NULL, SHORT_TIMEOUT);
                /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
            }

            if (RETCODE_OK == rcOpt)
            {
                param->AcT = AT_CXREG_ACT_INVALID;
                rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&param->AcT, ATTRANSCEIVER_DECIMAL, SHORT_TIMEOUT);
            }
            break;
        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR,
                         RETCODE_CELLULAR_RESPONSE_UNEXPECTED);
            break;
        }
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

Retcode_T At_Set_COPS(struct AtTransceiver_S *t, const AT_COPS_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = RETCODE_OK;

    switch (param->Mode)
    {
    case AT_COPS_MODE_AUTOMATIC:
    case AT_COPS_MODE_DEREGISTER:
        /* continue */
        break;
    case AT_COPS_MODE_MANUAL:
    case AT_COPS_MODE_SET_FORMAT_ONLY:
    case AT_COPS_MODE_MANUAL_THEN_AUTOMATIC:
        /* Currently not supported, sorry! ... maybe your first PR? :) */
        /** \todo Implement remaining 3GPP 27.007 command modes for AT+COPS
         * setter. */
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_3GPP27007_ATCOPS);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)param->Mode, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Set_CGDCONT(struct AtTransceiver_S *t, const AT_CGDCONT_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = RETCODE_OK;
    const char *pdpType = NULL;
    bool clearCtx = false;

    switch (param->PdpType)
    {
    case AT_CGDCONT_PDPTYPE_IP:
        pdpType = ARG_CGDCONT_PDPTYPE_IPIP;
        break;
    case AT_CGDCONT_PDPTYPE_IPV6:
        pdpType = ARG_CGDCONT_PDPTYPE_IPIPV6;
        break;
    case AT_CGDCONT_PDPTYPE_IPV4V6:
        pdpType = ARG_CGDCONT_PDPTYPE_IPIPV4V6;
        break;
    case AT_CGDCONT_PDPTYPE_INVALID:
        /* No <PDP_Type> means clear the context! */
        clearCtx = true;
        break;
    default:
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
        break;
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteSet(t, CMD_CGDCONT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, param->Cid, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && !clearCtx)
    {
        rc = AtTransceiver_WriteString(t, pdpType);
    }

    if (RETCODE_OK == rc && NULL != param->Apn)
    {
        rc = AtTransceiver_WriteString(t, param->Apn);
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

Retcode_T At_Set_CGACT(struct AtTransceiver_S *t, const AT_CGACT_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    Retcode_T rc = RETCODE_OK;

    rc = AtTransceiver_WriteSet(t, CMD_CGACT);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, param->State, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, param->Cid, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Set_CGPADDR(struct AtTransceiver_S *t, const AT_CGPADDR_Param_T *param, AT_CGPADDR_Resp_T *resp)
{
    assert(NULL != t);
    assert(NULL != param);
    assert(NULL != resp);

    Retcode_T rc = RETCODE_OK;
    char addressBuffer[MAX_IP_STR_LENGTH + 1];

    rc = AtTransceiver_WriteSet(t, CMD_CGPADDR);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteU8(t, param->Cid, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Set_CMEE(struct AtTransceiver_S *t, AT_CMEE_N_T n)
{
    assert(NULL != t);

    Retcode_T rc = RETCODE_OK;

    rc = AtTransceiver_WriteSet(t, CMD_CMEE);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)n, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Set_CPIN(struct AtTransceiver_S *t, const char *pin)
{
    assert(NULL != t);
    assert(NULL != pin);

    Retcode_T rc = RETCODE_OK;

    rc = AtTransceiver_WriteSet(t, CMD_3GPP27007_ATCPIN);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteString(t, pin);
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

Retcode_T At_Test_AT(struct AtTransceiver_S *t)
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

Retcode_T At_Set_ATE(struct AtTransceiver_S *t, bool enableEcho)
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

Retcode_T At_Set_CFUN(struct AtTransceiver_S *t, const AT_CFUN_Param_T *param)
{
    assert(NULL != t);
    assert(NULL != param);

    switch (param->Fun)
    {
    case AT_CFUN_FUN_MINIMUM:
    case AT_CFUN_FUN_FULL:
    case AT_CFUN_FUN_DISABLETX:
    case AT_CFUN_FUN_DISABLERX:
    case AT_CFUN_FUN_DISABLERXTX:
    case AT_CFUN_FUN_RESERVEDSTART:
    case AT_CFUN_FUN_RESERVEDEND:
    case AT_CFUN_FUN_PREPARESHUTDOWN:
        /* do nothing */
        break;
    case AT_CFUN_FUN_INVALID:
    default:
        if (AT_CFUN_FUN_RESERVEDSTART > param->Fun || AT_CFUN_FUN_RESERVEDEND < param->Fun)
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        }
        break;
    }

    Retcode_T rc = AtTransceiver_WriteSet(t, CMD_CFUN);

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)param->Fun, ATTRANSCEIVER_DECIMAL);
    }

    if (RETCODE_OK == rc && param->Rst != AT_CFUN_RST_INVALID)
    {
        rc = AtTransceiver_WriteI32(t, (int32_t)param->Rst, ATTRANSCEIVER_DECIMAL);
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

Retcode_T At_Get_CFUN(struct AtTransceiver_S *t, AT_CFUN_Resp_T *resp)
{
    assert(NULL != t);
    assert(NULL != resp);

    Retcode_T rc = AtTransceiver_WriteGet(t, CMD_CFUN);

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
        rc = AtTransceiver_ReadI32(t, (int32_t *)&resp->Fun, ATTRANSCEIVER_DECIMAL, CFUN_TIMEOUT);
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

Retcode_T At_HandleUrc_CREG(struct AtTransceiver_S *t)
{
    assert(NULL != t);

    AT_CREG_Param_T creg = {.N = AT_CXREG_N_INVALID,
                            .Stat = AT_CXREG_STAT_INVALID,
                            .Lac = AT_INVALID_LAC,
                            .Ci = AT_INVALID_CI,
                            .AcT = AT_CXREG_ACT_INVALID};
    Retcode_T rcOpt = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
    Retcode_T rc = AtTransceiver_ReadCommand(t, CMD_CREG, URC_TIMEOUT);
    if (RETCODE_OK != rc)
    {
        return RETCODE(RETCODE_SEVERITY_INFO, RETCODE_CELLULAR_URC_NOT_PRESENT);
    }

    rc = AtTransceiver_ReadI32(t, (int32_t *)&creg.Stat, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        switch (creg.Stat)
        {
        case AT_CXREG_STAT_HOME:
        case AT_CXREG_STAT_ROAMING:
        case AT_CXREG_STAT_CSFB_NOT_PREF_HOME:
        case AT_CXREG_STAT_CSFB_NOT_PREF_ROAMING:
            /* set device status and notify event */
            /** \todo Propagate type of CxREG to client-callback via param. */
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERED, NULL, 0);
            break;
        case AT_CXREG_STAT_NOT:
        case AT_CXREG_STAT_DENIED:
            Engine_NotifyNewState(CELLULAR_STATE_POWERON, NULL, 0);
            break;
        case AT_CXREG_STAT_NOT_AND_SEARCH:
        case AT_CXREG_STAT_UNKNOWN:
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERING, NULL, 0);
            break;
        default:
            /* do nothing */
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &creg.Lac, sizeof(creg.Lac), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &creg.Ci, sizeof(creg.Ci), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&creg.AcT, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, URC_TIMEOUT);
    }

    if (AT_INVALID_LAC != creg.Lac && AT_INVALID_CI != creg.Ci)
    {
        LOG_DEBUG("CREG stat:%d lac:%" PRIu16 " ci:%" PRIu32 " AcT:%d", (int)creg.Stat, creg.Lac, creg.Ci, creg.AcT);
    }
    else
    {
        LOG_DEBUG("CREG stat:%d", (int)creg.Stat);
    }

    return rc;
}

Retcode_T At_HandleUrc_CGREG(struct AtTransceiver_S *t)
{
    assert(NULL != t);

    AT_CGREG_Param_T cgreg = {.N = AT_CXREG_N_INVALID,
                              .Stat = AT_CXREG_STAT_INVALID,
                              .Lac = AT_INVALID_LAC,
                              .Ci = AT_INVALID_CI,
                              .AcT = AT_CXREG_ACT_INVALID,
                              .Rac = AT_INVALID_RAC};
    Retcode_T rcOpt = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
    Retcode_T rc = AtTransceiver_ReadCommand(t, CMD_CREG, URC_TIMEOUT);
    if (RETCODE_OK != rc)
    {
        return RETCODE(RETCODE_SEVERITY_INFO, RETCODE_CELLULAR_URC_NOT_PRESENT);
    }

    rc = AtTransceiver_ReadI32(t, (int32_t *)&cgreg.Stat, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        switch (cgreg.Stat)
        {
        case AT_CXREG_STAT_HOME:
        case AT_CXREG_STAT_ROAMING:
        case AT_CXREG_STAT_CSFB_NOT_PREF_HOME:
        case AT_CXREG_STAT_CSFB_NOT_PREF_ROAMING:
            /* set device status and notify event */
            /** \todo Propagate type of CxREG to client-callback via param. */
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERED, NULL, 0); //LCOV_EXCL_BR_LINE
            break;
        case AT_CXREG_STAT_NOT:
        case AT_CXREG_STAT_DENIED:
            Engine_NotifyNewState(CELLULAR_STATE_POWERON, NULL, 0); //LCOV_EXCL_BR_LINE
            break;
        case AT_CXREG_STAT_NOT_AND_SEARCH:
        case AT_CXREG_STAT_UNKNOWN:
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERING, NULL, 0); //LCOV_EXCL_BR_LINE
            break;
        default:
            /* do nothing */
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &cgreg.Lac, sizeof(cgreg.Lac), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &cgreg.Ci, sizeof(cgreg.Ci), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&cgreg.AcT, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadU8(t, &cgreg.Rac, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, URC_TIMEOUT);
    }

    if (AT_INVALID_LAC != cgreg.Lac && AT_INVALID_CI != cgreg.Ci)
    {
        LOG_DEBUG("CGREG stat:%d lac:%" PRIu16 " ci:%" PRIu32 " AcT:%d", (int)cgreg.Stat, cgreg.Lac, cgreg.Ci, cgreg.AcT);
    }
    else
    {
        LOG_DEBUG("CGREG stat:%d", (int)cgreg.Stat);
    }

    return rc;
}

Retcode_T At_HandleUrc_CEREG(struct AtTransceiver_S *t)
{
    assert(NULL != t);

    AT_CEREG_Param_T cereg = {.N = AT_CXREG_N_INVALID,
                              .Stat = AT_CXREG_STAT_INVALID,
                              .Tac = AT_INVALID_TAC,
                              .Ci = AT_INVALID_CI,
                              .AcT = AT_CXREG_ACT_INVALID};
    Retcode_T rcOpt = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
    Retcode_T rc = AtTransceiver_ReadCommand(t, CMD_CREG, URC_TIMEOUT);
    if (RETCODE_OK != rc)
    {
        return RETCODE(RETCODE_SEVERITY_INFO, RETCODE_CELLULAR_URC_NOT_PRESENT);
    }

    rc = AtTransceiver_ReadI32(t, (int32_t *)&cereg.Stat, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);

    if (RETCODE_OK == rc)
    {
        switch (cereg.Stat)
        {
        case AT_CXREG_STAT_HOME:
        case AT_CXREG_STAT_ROAMING:
        case AT_CXREG_STAT_CSFB_NOT_PREF_HOME:
        case AT_CXREG_STAT_CSFB_NOT_PREF_ROAMING:
            /* set device status and notify event */
            /** \todo Propagate type of CxREG to client-callback via param. */
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERED, NULL, 0);
            break;
        case AT_CXREG_STAT_NOT:
        case AT_CXREG_STAT_DENIED:
            Engine_NotifyNewState(CELLULAR_STATE_POWERON, NULL, 0);
            break;
        case AT_CXREG_STAT_NOT_AND_SEARCH:
        case AT_CXREG_STAT_UNKNOWN:
            Engine_NotifyNewState(CELLULAR_STATE_REGISTERING, NULL, 0);
            break;
        default:
            /* do nothing */
            break;
        }
    }
    if (RETCODE_OK == rc)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &cereg.Tac, sizeof(cereg.Tac), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadHexString(t, &cereg.Ci, sizeof(cereg.Ci), NULL, URC_TIMEOUT);
        /** \todo: wrong endianess + false FF in value, if bytesRead < sizeof(value) */
    }

    if (RETCODE_OK == rcOpt)
    {
        rcOpt = AtTransceiver_ReadI32(t, (int32_t *)&cereg.AcT, ATTRANSCEIVER_DECIMAL, URC_TIMEOUT);
    }

    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_SkipArgument(t, URC_TIMEOUT);
    }

    if (AT_INVALID_LAC != cereg.Tac && AT_INVALID_CI != cereg.Ci)
    {
        LOG_DEBUG("CEREG stat:%d tac:%" PRIu16 " ci:%" PRIu32 " AcT:%d", (int)cereg.Stat, cereg.Tac, cereg.Ci, cereg.AcT);
    }
    else
    {
        LOG_DEBUG("CEREG stat:%d", (int)cereg.Stat);
    }

    return rc;
}
