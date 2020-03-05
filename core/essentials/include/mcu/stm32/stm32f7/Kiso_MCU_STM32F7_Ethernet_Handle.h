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
 * @ingroup KISO_HAL_MCU_ETHERNET
 * @{
 *
 * @file
 *
 * @brief Defines the handle structure for STM32F7 Ethernet peripherals.
 */

#ifndef KISO_MCU_STM32F7_ETHERNET_H_
#define KISO_MCU_STM32F7_ETHERNET_H_

#include "Kiso_MCU_Ethernet.h"

#if KISO_FEATURE_ETHERNET

#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_eth.h"

enum MCU_Ethernet_State_E
{
    ETHERNET_STATE_UNINITIALIZED = 0,
    ETHERNET_STATE_INITIALIZED = 1 << 0,
    ETHERNET_STATE_TX_ONGOING = 1 << 1,
    ETHERNET_STATE_TX_IDLE = 1 << 2,
    ETHERNET_STATE_RX_ONGOING = 1 << 3
};

struct MCU_Ethernet_S
{
    ETH_HandleTypeDef VendorHandle;
    enum KISO_HAL_TransferMode_E TransferMode;
    MCU_Ethernet_EventCallback_T EventCallback;
    enum MCU_Ethernet_State_E State;
    size_t NextFrameOffset;
    struct MCU_Ethernet_FrameBuffer_S *RxPool;
    size_t RxPoolLength;

    Retcode_T (*TransmitFrame)(struct MCU_Ethernet_S *eth);
    Retcode_T (*StartReceive)(struct MCU_Ethernet_S *eth);
    Retcode_T (*StopReceive)(struct MCU_Ethernet_S *eth);
};

#endif /* KISO_FEATURE_ETHERNET */
#endif /* KISO_MCU_STM32F7_ETHERNET_H_ */

/** @} */
