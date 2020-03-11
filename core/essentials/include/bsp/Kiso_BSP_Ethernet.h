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
 *    Robert Bosch GmbH - initial contribution
 *
 ******************************************************************************/

/**
 * @defgroup KISO_HAL_BSP_ETHERNET Ethernet
 * @ingroup KISO_HAL_BSP_IF
 * @{
 * @brief BSP for on-chip Ethernet-MAC peripherals
 *
 * @file
 * @brief BSP API declaration. To be defined in board specific BSP
 * implementation.
 * @}
 *
 */

#ifndef KISO_BSP_ETHERNET_H_
#define KISO_BSP_ETHERNET_H_

#include "Kiso_HAL.h"

#if KISO_FEATURE_BSP_ETHERNET

#include "Kiso_Retcode.h"

Retcode_T BSP_Ethernet_Connect(void);

Retcode_T BSP_Ethernet_Enable(void);

HWHandle_T BSP_Ethernet_GetEthernetHandle(void);

Retcode_T BSP_Ethernet_Disable(void);

Retcode_T BSP_Ethernet_Disconnect(void);

#endif /* KISO_FEATURE_BSP_ETHERNET */

#endif /* KISO_BSP_ETHERNET_H_ */