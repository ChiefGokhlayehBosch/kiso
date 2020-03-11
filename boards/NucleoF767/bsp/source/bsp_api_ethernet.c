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

#include "Kiso_BSP_Ethernet.h"

#if KISO_FEATURE_BSP_ETHERNET

#include "BSP_NucleoF767.h"
#undef KISO_MODULE_ID
#define KISO_MODULE_ID MODULE_BSP_ETHERNET

#include "Kiso_MCU_STM32F7_Ethernet_Handle.h"

#include "protected/gpio.h"

#include "stm32f7xx_hal_conf.h"
#include "stm32f7xx_hal_eth.h"

#define ETHERNET_INTERRUPT_PRIORITY (7U)
#define ETHERNET_INTERRUPT_SUBPRIORITY (0U)

extern void ETH_IRQHandler(void);

static struct MCU_Ethernet_S EthernetDevice;

static ETH_DMADescTypeDef TxDmaDescriptors[ETH_TXBUFNB] __attribute__((section(".TxDecripSection")));
static ETH_DMADescTypeDef RxDmaDescriptors[ETH_RXBUFNB] __attribute__((section(".RxDecripSection")));

static uint8_t TxBuffers[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((section(".TxArraySection")));
static uint8_t RxBuffers[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((section(".RxArraySection")));

static uint8_t DummyMacAddress[6] = {0, 0, 0, 0, 0, 0};

static int BspState = BSP_STATE_INIT;

void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&EthernetDevice.VendorHandle);
}

Retcode_T BSP_Ethernet_Connect(void)
{
    if (!(BspState & BSP_STATE_TO_CONNECTED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    EthernetDevice.State = ETHERNET_STATE_UNINITIALIZED;
    EthernetDevice.TransferMode = KISO_HAL_TRANSFER_MODE_INTERRUPT;

    EthernetDevice.VendorHandle.Instance = ETH;
    EthernetDevice.VendorHandle.Init.MACAddr = DummyMacAddress;
    EthernetDevice.VendorHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    EthernetDevice.VendorHandle.Init.Speed = ETH_SPEED_100M;
    EthernetDevice.VendorHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
    EthernetDevice.VendorHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
    EthernetDevice.VendorHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
    EthernetDevice.VendorHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
    EthernetDevice.VendorHandle.Init.PhyAddress = DP83848_PHY_ADDRESS;

    GPIO_InitTypeDef gpio;
    gpio.Speed = GPIO_SPEED_HIGH;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF11_ETH;
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    GPIO_OpenClockGate(GPIO_PORT_A, gpio.Pin);
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_13;
    GPIO_OpenClockGate(GPIO_PORT_B, gpio.Pin);
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_OpenClockGate(GPIO_PORT_C, gpio.Pin);
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13;
    GPIO_OpenClockGate(GPIO_PORT_G, gpio.Pin);
    HAL_GPIO_Init(GPIOG, &gpio);

    BspState = BSP_STATE_CONNECTED;

    return RETCODE_OK;
}

Retcode_T BSP_Ethernet_Enable(void)
{
    if (!(BspState & BSP_STATE_TO_ENABLED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    __HAL_RCC_ETH_CLK_ENABLE();

    HAL_NVIC_SetPriority(ETH_IRQn, ETHERNET_INTERRUPT_PRIORITY, ETHERNET_INTERRUPT_SUBPRIORITY);
    HAL_NVIC_EnableIRQ(ETH_IRQn);

    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_4);

    HAL_StatusTypeDef halStat = HAL_ETH_Init(&EthernetDevice.VendorHandle);
    if (HAL_OK != halStat)
    {
        return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_FAILURE);
    }

    halStat = HAL_ETH_DMATxDescListInit(&EthernetDevice.VendorHandle, TxDmaDescriptors, &TxBuffers[0][0], ETH_TXBUFNB);
    if (HAL_OK != halStat)
    {
        return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_FAILURE);
    }

    halStat = HAL_ETH_DMARxDescListInit(&EthernetDevice.VendorHandle, RxDmaDescriptors, &RxBuffers[0][0], ETH_RXBUFNB);
    if (HAL_OK != halStat)
    {
        return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_FAILURE);
    }

    BspState = BSP_STATE_TO_ENABLED;

    return RETCODE_OK;
}

HWHandle_T BSP_Ethernet_GetEthernetHandle(void)
{
    return &EthernetDevice;
}

Retcode_T BSP_Ethernet_Disable(void)
{
    /** @todo Implement disable. */
    return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
}

Retcode_T BSP_Ethernet_Disconnect(void)
{
    /** @todo Implement disconnect. */
    return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
}

#endif /* KISO_FEATURE_BSP_ETHERNET */
