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

#ifndef ENGINE_TH_HH_
#define ENGINE_TH_HH_

#include <gtest.h>

#include "Engine.h"

FAKE_VALUE_FUNC(Retcode_T, Engine_Initialize, Cellular_StateChanged_T)
FAKE_VOID_FUNC(Engine_Deinitialize)
FAKE_VOID_FUNC(Engine_NotifyNewState, Cellular_State_T, void *, uint32_t)
FAKE_VOID_FUNC(Engine_SetEchoMode, bool)
FAKE_VALUE_FUNC(bool, Engine_GetEchoMode)
FAKE_VALUE_FUNC(Retcode_T, Engine_OpenTransceiver, struct AtTransceiver_S **)
FAKE_VALUE_FUNC(Retcode_T, Engine_CloseTransceiver)

#endif /* ENGINE_TH_HH_ */
