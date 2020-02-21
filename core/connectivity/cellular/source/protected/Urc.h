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

/**
 * @ingroup KISO_CELLULAR_COMMON
 * @defgroup URC URC
 * @{
 *
 * @brief Interface to variant specific code performing URC handling.
 *
 * The @ref ENGINE will use this interface to call for handling of
 * unsolicited-response-codes (URCs).
 *
 * @file
 */

#ifndef URC_H_
#define URC_H_

#include "AtTransceiver.h"

/**
 * @brief Performs URC parsing and interpretation provided by the selected
 * variant.
 *
 * This function is called only after receiving unsolicited data over the
 * underlying AT communications channel. A implementor must take note that the
 * transceiver may only be used for the duration of this functions lifetime.
 *
 * @note Each variant has to implement this function for themselves.
 *
 * @param[in,out] t
 * A locked and ready-to-read transceiver instance, to be used for parsing the
 * individual URC elements.
 */
void Urc_HandleResponses(struct AtTransceiver_S *t);

#endif /* URC_H_ */

/** @} */
