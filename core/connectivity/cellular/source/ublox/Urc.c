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

#include "UBlox.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_URC

#include "Kiso_CellularConfig.h"
#ifdef CELLULAR_VARIANT_UBLOX

#include "Urc.h"

#include "AtTransceiver.h"
#include "At3Gpp27007.h"
#include "AtUBlox.h"

#include "Kiso_Basics.h"
#include "Kiso_Retcode.h"
#include "Kiso_Assert.h"

#include "Kiso_Logging.h"

#include "FreeRTOS.h"
#include "portmacro.h"

#define URC_CMD_BUFFER_SIZE (8U)

#define URC_SHORT_TIMEOUT (pdMS_TO_TICKS(100))

#define URC_SCAN_LIMIT (2U)

#define URC_MIN(a, b) ((a) < (b) ? (a) : (b))

void Urc_HandleResponses(struct AtTransceiver_S *t)
{
    Retcode_T rc = RETCODE_OK;
    char cmd[URC_CMD_BUFFER_SIZE];
    size_t cmdLen = 0;
    unsigned int scanCount = 0;

    do
    {
        rc = AtTransceiver_ReadCommandAny(t, cmd, &cmdLen, URC_SHORT_TIMEOUT);
        if (RETCODE_TIMEOUT == Retcode_GetCode(rc) && cmdLen > 0U)
        {
            LOG_ERROR("Timeout during unfinished URC cmd: %.*s", cmdLen, cmd);
            Retcode_RaiseError(rc);
        }
        else if (RETCODE_OK != rc)
        {
            LOG_ERROR("Miscellaneous error while reading URC cmd: 0x%08x, %.*s", rc, cmdLen, cmd);
            Retcode_RaiseError(rc);
        }

        if (RETCODE_OK == rc)
        {
            if (0U == strncmp(cmd, AT3GPP27007_URC_CREG, URC_MIN(cmdLen, URC_CMD_BUFFER_SIZE)))
            {
                At3Gpp27007_CREG_Resp_T resp;
                rc = At3Gpp27007_UrcCREG(t, &resp);
                if (RETCODE_OK == rc)
                {
                    /** @todo Implement */
                }
            }
            else if (0U == strncmp(cmd, AT3GPP27007_URC_CGREG, URC_MIN(cmdLen, URC_CMD_BUFFER_SIZE)))
            {
                At3Gpp27007_CGREG_Resp_T resp;
                rc = At3Gpp27007_UrcCGREG(t, &resp);
                if (RETCODE_OK == rc)
                {
                    /** @todo Implement */
                }
            }
            else if (0U == strncmp(cmd, AT3GPP27007_URC_CEREG, URC_MIN(cmdLen, URC_CMD_BUFFER_SIZE)))
            {
                At3Gpp27007_CEREG_Resp_T resp;
                rc = At3Gpp27007_UrcCEREG(t, &resp);
                if (RETCODE_OK == rc)
                {
                    /** @todo Implement */
                }
            }
        }
    } while (RETCODE_TIMEOUT != Retcode_GetCode(rc) && ++scanCount < URC_SCAN_LIMIT);
}

#endif /* CELLULAR_VARIANT_UBLOX */
