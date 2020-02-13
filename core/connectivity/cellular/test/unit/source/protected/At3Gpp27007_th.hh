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

#ifndef AT_3GPP_27007_TH_HH_
#define AT_3GPP_27007_TH_HH_

#include "At3Gpp27007.h"

#include "gtest.h"

/* *** NETWORK COMMANDS ***************************************************** */
FAKE_VALUE_FUNC(Retcode_T, At_Set_CREG, struct AtTransceiver_S *, AT_CXREG_N_T)
FAKE_VALUE_FUNC(Retcode_T, At_Set_CGREG, struct AtTransceiver_S *, AT_CXREG_N_T)
FAKE_VALUE_FUNC(Retcode_T, At_Set_CEREG, struct AtTransceiver_S *, AT_CXREG_N_T)
FAKE_VALUE_FUNC(Retcode_T, At_Get_CREG, struct AtTransceiver_S *, AT_CREG_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Get_CGREG, struct AtTransceiver_S *, AT_CGREG_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Get_CEREG, struct AtTransceiver_S *, AT_CEREG_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Set_COPS, struct AtTransceiver_S *, const AT_COPS_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Set_CGDCONT, struct AtTransceiver_S *, const AT_CGDCONT_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Set_CGACT, struct AtTransceiver_S *, const AT_CGACT_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Set_CGPADDR, struct AtTransceiver_S *, const AT_CGPADDR_Param_T *, AT_CGPADDR_Resp_T *)
/* *** ERROR HANDLING ******************************************************* */
FAKE_VALUE_FUNC(Retcode_T, At_Set_CMEE, struct AtTransceiver_S *, AT_CMEE_N_T);
/* *** SIM COMMANDS ********************************************************* */
FAKE_VALUE_FUNC(Retcode_T, At_Set_CPIN, struct AtTransceiver_S *, const char *)
/* *** TE-TA INTERFACE COMMANDS ********************************************* */
FAKE_VALUE_FUNC(Retcode_T, At_Test_AT, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(Retcode_T, At_Set_ATE, struct AtTransceiver_S *, bool)
/* *** POWER CONTROL COMMANDS *********************************************** */
FAKE_VALUE_FUNC(Retcode_T, At_Set_CFUN, struct AtTransceiver_S *, const AT_CFUN_Param_T *)
FAKE_VALUE_FUNC(Retcode_T, At_Get_CFUN, struct AtTransceiver_S *, AT_CFUN_Resp_T *)
/* *** URC HANDLERS ********************************************************* */
FAKE_VALUE_FUNC(Retcode_T, At_HandleUrc_CREG, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(Retcode_T, At_HandleUrc_CGREG, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(Retcode_T, At_HandleUrc_CEREG, struct AtTransceiver_S *)

#endif /* AT_3GPP_27007_TH_HH_ */
