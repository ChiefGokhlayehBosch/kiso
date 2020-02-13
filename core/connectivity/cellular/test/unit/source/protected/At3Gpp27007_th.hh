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

#ifndef AT3GPP27007_TH_HH_
#define AT3GPP27007_TH_HH_

#include <fff.h>

#include "At3Gpp27007.h"

/* *** NETWORK COMMANDS ***************************************************** */
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCREG, struct AtTransceiver_S *, const At3Gpp27007_CXREG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCGREG, struct AtTransceiver_S *, const At3Gpp27007_CXREG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCEREG, struct AtTransceiver_S *, const At3Gpp27007_CXREG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_GetCREG, struct AtTransceiver_S *, At3Gpp27007_CREG_GetResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_GetCGREG, struct AtTransceiver_S *, At3Gpp27007_CGREG_GetResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_GetCEREG, struct AtTransceiver_S *, At3Gpp27007_CEREG_GetResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCMEE, struct AtTransceiver_S *, const At3Gpp27007_CMEE_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCOPS, struct AtTransceiver_S *, const At3Gpp27007_COPS_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCGDCONT, struct AtTransceiver_S *, const At3Gpp27007_CGDCONT_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCGACT, struct AtTransceiver_S *, const At3Gpp27007_CGACT_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_QueryCGPADDR, struct AtTransceiver_S *, const At3Gpp27007_CGPADDR_Query_T *, At3Gpp27007_CGPADDR_QueryResponse_T *)
/* *** SIM COMMANDS ********************************************************* */
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCPIN, struct AtTransceiver_S *, const At3Gpp27007_CPIN_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_GetCPIN, struct AtTransceiver_S *, At3Gpp27007_CPIN_GetResponse_T *)
/* *** TE-TA INTERFACE COMMANDS ********************************************* */
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_ExecuteAT, struct AtTransceiver_S *);
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_ExecuteATE, struct AtTransceiver_S *, bool)
/* *** POWER CONTROL COMMANDS *********************************************** */
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_SetCFUN, struct AtTransceiver_S *, const At3Gpp27007_CFUN_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_GetCFUN, struct AtTransceiver_S *, At3Gpp27007_CFUN_GetResponse_T *)
/* *** URC HANDLERS ********************************************************* */
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_UrcCREG, struct AtTransceiver_S *, At3Gpp27007_CREG_UrcResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_UrcCGREG, struct AtTransceiver_S *, At3Gpp27007_CGREG_UrcResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, At3Gpp27007_UrcCEREG, struct AtTransceiver_S *, At3Gpp27007_CEREG_UrcResponse_T *)

#endif /* AT3GPP27007_TH_HH_ */
