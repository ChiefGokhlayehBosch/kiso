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
 * @ingroup KISO_CELLULAR
 * @defgroup KISO_CELLULAR_VARIANTS Variants
 * @{
 * @brief Supported variants implementing the Cellular API
 *
 * Use `#define CELLULAR_VARIANT_UBLOX` in @c Kiso_CellularConfig.h to select
 * the u-blox variant.
 *
 * Use `#define CELLULAR_VARIANT_QUECTEL` in @c Kiso_CellularConfig.h to select
 * the Quectel variant.
 *
 * @note If no variant is defined in the config header, an build-error will be
 * thrown.
 * @note Only one variant may be selected at a time.
 *
 * @file
 */

#ifndef VARIANT_H_
#define VARIANT_H_

#include "Kiso_CellularConfig.h"

#if defined(CELLULAR_VARIANT_UBLOX)
#include "ublox/UBlox.h"
#else if defined(CELLULAR_VARIANT_QUECTEL)
#include "quectel/Quectel.h"
#else
#error No Cellular variant selected. Please select which variant to compile in "Kiso_CellularConfig.h"
#endif

#endif /* VARIANT_H_ */

/** @} */
