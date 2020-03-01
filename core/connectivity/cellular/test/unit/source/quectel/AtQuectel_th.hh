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
 *    Robert Bosch GmbH -
 *
 ******************************************************************************/

#ifndef ATQUECTEL_TH_HH_
#define ATQUECTEL_TH_HH_

#include "AtQuectel.h"

#include <fff.h>

FAKE_VALUE_FUNC(Retcode_T, AtQuectel_QueryQCFG, struct AtTransceiver_S *, const AtQuectel_QCFG_Query_T *, AtQuectel_QCFG_QueryResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQCFG, struct AtTransceiver_S *, const AtQuectel_QCFG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_QueryQURCCFG, struct AtTransceiver_S *, const AtQuectel_QURCCFG_Query_T *, AtQuectel_QURCCFG_QueryResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQURCCFG, struct AtTransceiver_S *, const AtQuectel_QURCCFG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_ExecuteQCCID, struct AtTransceiver_S *, AtQuectel_QCCID_ExecuteResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_QueryQINDCFG, struct AtTransceiver_S *, const AtQuectel_QINDCFG_Query_T *, AtQuectel_QINDCFG_QueryResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQINDCFG, struct AtTransceiver_S *, const AtQuectel_QINDCFG_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_ExecuteQINISTAT, struct AtTransceiver_S *, AtQuectel_QINISTAT_ExecuteResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_QueryQICSGP, struct AtTransceiver_S *, const AtQuectel_QICSGP_Query_T *, AtQuectel_QICSGP_QueryResponse_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQICSGP, struct AtTransceiver_S *, const AtQuectel_QICSGP_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQIACT, struct AtTransceiver_S *, const AtQuectel_QIACT_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_GetQIACT, struct AtTransceiver_S *, AtQuectel_QIACT_GetResponse_T *, size_t, size_t *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQIDEACT, struct AtTransceiver_S *, const AtQuectel_QIDEACT_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQIOPEN, struct AtTransceiver_S *, const AtQuectel_QIOPEN_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_SetQICLOSE, struct AtTransceiver_S *, const AtQuectel_QICLOSE_Set_T *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_GetQISTATE, struct AtTransceiver_S *, AtQuectel_QISTATE_GetResponse_T *, size_t, size_t *)
FAKE_VALUE_FUNC(Retcode_T, AtQuectel_QueryQISTATE, struct AtTransceiver_S *, const AtQuectel_QISTATE_Query_T *, AtQuectel_QISTATE_QueryResponse_T *, size_t, size_t *)

#endif /* ATQUECTEL_TH_HH_ */
