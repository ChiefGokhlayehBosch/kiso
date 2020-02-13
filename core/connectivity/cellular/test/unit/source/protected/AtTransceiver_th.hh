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
 *
 ******************************************************************************/

#ifndef ATTRANSCEIVER_TH_HH_
#define ATTRANSCEIVER_TH_HH_

#include "AtTransceiver.h"

#include "fff.h"

FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Initialize, struct AtTransceiver_S *, void *, size_t, AtTransceiver_WriteFunction_T)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Lock, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_TryLock, struct AtTransceiver_S *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_PrepareWrite, struct AtTransceiver_S *, enum AtTransceiver_WriteOption_E, void *, size_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteAction, struct AtTransceiver_S *, const char *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteSet, struct AtTransceiver_S *, const char *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteGet, struct AtTransceiver_S *, const char *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Write, struct AtTransceiver_S *, const void *, size_t, enum AtTransceiver_WriteState_E)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteI8, struct AtTransceiver_S *, int8_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteU8, struct AtTransceiver_S *, uint8_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteI16, struct AtTransceiver_S *, int16_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteU16, struct AtTransceiver_S *, uint16_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteI32, struct AtTransceiver_S *, int32_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteU32, struct AtTransceiver_S *, uint32_t, int)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteString, struct AtTransceiver_S *, const char *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_WriteHexString, struct AtTransceiver_S *, const void *, size_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Flush, struct AtTransceiver_S *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_SkipBytes, struct AtTransceiver_S *, size_t, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_SkipArgument, struct AtTransceiver_S *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_SkipLine, struct AtTransceiver_S *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadCommandAny, struct AtTransceiver_S *, char *, size_t, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadCommand, struct AtTransceiver_S *, const char *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Read, struct AtTransceiver_S *, void *, size_t, size_t *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadArgument, struct AtTransceiver_S *, char *, size_t, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadString, struct AtTransceiver_S *, char *, size_t, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadHexString, struct AtTransceiver_S *, void *, size_t, size_t *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadI8, struct AtTransceiver_S *, int8_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadU8, struct AtTransceiver_S *, uint8_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadI16, struct AtTransceiver_S *, int16_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadU16, struct AtTransceiver_S *, uint16_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadI32, struct AtTransceiver_S *, int32_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadU32, struct AtTransceiver_S *, uint32_t *, int, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_ReadCode, struct AtTransceiver_S *, enum AtTransceiver_ResponseCode_E *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_CheckEndOfLine, struct AtTransceiver_S *, bool *, TickType_t)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Unlock, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(Retcode_T, AtTransceiver_Feed, struct AtTransceiver_S *, const void *, size_t, size_t *)
FAKE_VOID_FUNC(AtTransceiver_Deinitialize, struct AtTransceiver_S *)
FAKE_VALUE_FUNC(int, AtTransceiver_ResponseCodeAsNumeric, enum AtTransceiver_ResponseCode_E)
FAKE_VALUE_FUNC(const char *, AtTransceiver_ResponseCodeAsString, enum AtTransceiver_ResponseCode_E)

#endif
