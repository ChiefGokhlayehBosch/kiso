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

/**
 * @ingroup KISO_CELLULAR_VARIANTS
 * @defgroup KISO_CELLULAR_VARIANT_UBLOX u-blox
 * @{
 * @brief Variant implementation for u-blox
 *
 * @file
 */

#ifndef UBLOX_H_
#define UBLOX_H_

#include "Kiso_CellularModules.h"

enum UBlox_ModuleId_E
{
    KISO_CELLULAR_MODULE_ID_ATUBLOX = KISO_CELLULAR_MODULE_ID_STARTOFVARIANT,

    KISO_CELLULAR_MODULE_ID_INIT,
    KISO_CELLULAR_MODULE_ID_NETWORK,
    KISO_CELLULAR_MODULE_ID_POWER,
    KISO_CELLULAR_MODULE_ID_UBLOXUTILS,
    KISO_CELLULAR_MODULE_ID_URC,

    KISO_CELLULAR_MODULE_ID_DNS_SERVICE,
    KISO_CELLULAR_MODULE_ID_SMS_SERVICE,
    KISO_CELLULAR_MODULE_ID_SOCKET_SERVICE,
    KISO_CELLULAR_MODULE_ID_HTTP_SERVICE,
};

#endif /* UBLOX_H_ */

/** @} */
