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

#include "Kiso_MCU_Ethernet.h"

#if KISO_FEATURE_ETHERNET

#include "Kiso_MCU_STM32F7_Ethernet_Handle.h"

#include "Kiso_Retcode.h"

#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_eth.h"

/**
 * @brief Mapper for HAL function return values.
 *
 * @param[in] halRet
 * Vendor driver return code
 *
 * @return The mapped #Retcode_T for the given HAL status.
 */
static inline Retcode_T MapHalStatusToRetcode(HAL_StatusTypeDef halRet);
/**
 * @brief Map a given STM #ETH_HandleTypeDef to its corresponding
 * #MCU_Ethernet_S by up-casting the structures base-address.
 *
 * @param[in] heth
 * Vendor handle to convert.
 *
 * @return Pointer to the corresponding #MCU_Ethernet_S.
 */
static inline struct MCU_Ethernet_S *MapHalHandleToMcuDevice(ETH_HandleTypeDef *heth);

/**
 * @brief Transmit an Ethernet frame in interrupt mode.
 *
 * The frame to be transmitted must be constructed through preceeding calls to
 * #MCU_Ethernet_AppendToNextFrame().
 *
 * @param[in,out] eth
 * MCU handle to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
static Retcode_T TransmitFrameInInterruptMode(struct MCU_Ethernet_S *eth);

/**
 * @brief Start the rx-process in interrupt mode.
 *
 * @param[in,out] eth
 * MCU handle to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
static Retcode_T StartReceiveInInterruptMode(struct MCU_Ethernet_S *eth);

/**
 * @brief Stop the rx-process in interrupt mode.
 *
 * @param[in,out] eth
 * MCU handle to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
static Retcode_T StopReceiveInInterruptMode(struct MCU_Ethernet_S *eth);

/**
 * @brief Cancels transmission and shuts down the peripherals transmit process.
 *
 * @param[in,out] eth
 * MCU handle to work with.
 */
static void CancelTransmit(struct MCU_Ethernet_S *eth);

/* These functions are usually declared static inside the STM32CubeF7. We made
 * them publicly linkable by removing the static keyword in the original
 * declaration/definition.
 *
 * If the cube version changes and this no longer links, consider changing the
 * original function declarations/definitions to externally linkable. */
extern void ETH_MACAddressConfig(ETH_HandleTypeDef *heth, uint32_t MacAddr, const uint8_t *Addr);
extern void ETH_MACReceptionEnable(ETH_HandleTypeDef *heth);
extern void ETH_MACReceptionDisable(ETH_HandleTypeDef *heth);
extern void ETH_MACTransmissionEnable(ETH_HandleTypeDef *heth);
extern void ETH_MACTransmissionDisable(ETH_HandleTypeDef *heth);
extern void ETH_DMATransmissionEnable(ETH_HandleTypeDef *heth);
extern void ETH_DMATransmissionDisable(ETH_HandleTypeDef *heth);
extern void ETH_DMAReceptionEnable(ETH_HandleTypeDef *heth);
extern void ETH_DMAReceptionDisable(ETH_HandleTypeDef *heth);
extern void ETH_FlushTransmitFIFO(ETH_HandleTypeDef *heth);

static inline Retcode_T MapHalStatusToRetcode(HAL_StatusTypeDef halRet)
{
    Retcode_T retcode;
    switch (halRet)
    {
    case HAL_OK:
        retcode = RETCODE_OK;
        break;
    case HAL_BUSY:
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
        break;
    case HAL_TIMEOUT:
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
        break;
    case HAL_ERROR:
        /* Fall Through */
    default:
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
        break;
    }
    return retcode;
}

static inline struct MCU_Ethernet_S *MapHalHandleToMcuDevice(ETH_HandleTypeDef *heth)
{
    /* The cast from ETH_HandleTypeDef to MCU_Ethernet_S is only safe if
     * MCU_Ethernet_S::VendorHandle is the first member in the struct. Thus,
     * VendorHandle and heth have the same start address in memory, meaning
     * we can up-cast the handle to get the whole struct. */
    return (struct MCU_Ethernet_S *)heth;
}

static Retcode_T TransmitFrameInInterruptMode(struct MCU_Ethernet_S *eth)
{
    /* First time setup if the underlying tx-proces is not yet started up. */
    if (!(eth->State & ETHERNET_STATE_TX_IDLE))
    {
        ETH_MACTransmissionEnable(&eth->VendorHandle);
        ETH_FlushTransmitFIFO(&eth->VendorHandle);
        ETH_DMATransmissionEnable(&eth->VendorHandle);
    }

    eth->State |= ETHERNET_STATE_TX_ONGOING;

    HAL_StatusTypeDef halStat = MapHalStatusToRetcode(HAL_ETH_TransmitFrame(&eth->VendorHandle, eth->NextFrameOffset));

    /* Do always: When "transmit underflow"-bit set, clear it and issue
     * "transmit poll demand" to resume transmission.
     *
     * While the DMA is busy shuffling through the DMA descriptor chain, the
     * Ethernet MAC may starve due to a lack of incoming data. It will indicate
     * this via "transmit underflow"-bit. */
    if (eth->VendorHandle.Instance->DMASR & ETH_DMASR_TUS)
    {
        /* Clear transmit underflow status flag. */
        eth->VendorHandle.Instance->DMASR &= ~ETH_DMASR_TUS;

        /* Resume DMA transmission by writing some (really any) value into the
         * "Ethernet DMA transmit poll demand" register. */
        eth->VendorHandle.Instance->DMATPDR = 0;
    }

    return halStat;
}

static Retcode_T StartReceiveInInterruptMode(struct MCU_Ethernet_S *eth)
{
    eth->State |= ETHERNET_STATE_RX_ONGOING;

    ETH_MACReceptionEnable(&eth->VendorHandle);
    ETH_DMAReceptionEnable(&eth->VendorHandle);

    return RETCODE_OK;
}

static Retcode_T StopReceiveInInterruptMode(struct MCU_Ethernet_S *eth)
{
    ETH_DMAReceptionDisable(&eth->VendorHandle);
    ETH_MACReceptionDisable(&eth->VendorHandle);

    eth->State &= ~ETHERNET_STATE_RX_ONGOING;

    return RETCODE_OK;
}

static void CancelTransmit(struct MCU_Ethernet_S *eth)
{
    ETH_DMATransmissionDisable(&eth->VendorHandle);
    ETH_FlushTransmitFIFO(&eth->VendorHandle);
    ETH_MACTransmissionDisable(&eth->VendorHandle);
}

/* See stm32f7xx_hal_eth.h for function declaration. */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
    struct MCU_Ethernet_S *eth = MapHalHandleToMcuDevice(heth);
    struct MCU_Ethernet_Event_S event;
    memset(&event, 0U, sizeof(struct MCU_Ethernet_Event_S));
    event.TxStopped = 1;
    event.TxNextFrameReady = 1;
    event.IsIsr = 1;

    eth->State &= ~ETHERNET_STATE_TX_ONGOING;
    eth->State |= ETHERNET_STATE_TX_IDLE;

    eth->EventCallback(eth, event);
}

/* See stm32f7xx_hal_eth.h for function declaration. */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    struct MCU_Ethernet_S *eth = MapHalHandleToMcuDevice(heth);
    struct MCU_Ethernet_Event_S event;
    memset(&event, 0U, sizeof(struct MCU_Ethernet_Event_S));
    event.RxAvailable = 1;
    event.IsIsr = 1;

    eth->EventCallback(eth, event);
}

/* See stm32f7xx_hal_eth.h for function declaration. */
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
    struct MCU_Ethernet_S *eth = MapHalHandleToMcuDevice(heth);
    struct MCU_Ethernet_Event_S event;
    memset(&event, 0U, sizeof(struct MCU_Ethernet_Event_S));

    event.TxStopped = __HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_TPS);
    event.RxStopped = __HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_RPS);
    if (__HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_TJT) ||
        __HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_TU))
    {
        event.TxError = 1;
        event.DataLoss = 1;
    }
    else if (__HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_RO) ||
             __HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_RWT))
    {
        event.RxError = 1;
        event.DataLoss = 1;
    }
    else if (__HAL_ETH_DMA_GET_FLAG(heth, ETH_DMA_FLAG_RBU))
    {
        event.RxError = 1;
        event.DataLoss = 1;
    }
    event.IsIsr = 1;

    if (event.RxStopped)
    {
        eth->State &= ~ETHERNET_STATE_RX_ONGOING;
    }
    if (event.TxStopped)
    {
        eth->State &= ~ETHERNET_STATE_TX_ONGOING;
        eth->State |= ETHERNET_STATE_TX_IDLE;
    }

    eth->EventCallback(eth, event);
}

Retcode_T MCU_Ethernet_Initialize(Ethernet_T eth, MCU_Ethernet_EventCallback_T callback)
{
    if (NULL == eth || NULL == callback)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;

    /* This sanity check should ensure that the cast performed in
     * MapHalHandleToMcuDevice() will work. Unfortunately this is done only at
     * runtime, a solution with static_assert() would therefore be much
     * appreciated. */
    assert((void *)mcuEth == (void *)&mcuEth->VendorHandle);

    Retcode_T rc = RETCODE_OK;

    if (ETHERNET_STATE_UNINITIALIZED != mcuEth->State)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    if (RETCODE_OK == rc)
    {
        switch (mcuEth->TransferMode)
        {
        case KISO_HAL_TRANSFER_MODE_POLLING:
            /* Feature is possible with STM hardware, but currently not
             * implemented by MCU Ethernet. */
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
            break;
        case KISO_HAL_TRANSFER_MODE_INTERRUPT:
            mcuEth->TransmitFrame = TransmitFrameInInterruptMode;
            mcuEth->StartReceive = StartReceiveInInterruptMode;
            mcuEth->StopReceive = StopReceiveInInterruptMode;
            break;
        case KISO_HAL_TRANSFER_MODE_DMA:
            /* Ethernet on STM hardware is ALWAYS DMA driven. For this purpose
             * STM uses a dedicated Ethernet-DMA controller.
             * The KISO_HAL_TRANSFER_MODE_DMA-mode was meant for cases, where
             * the DMA may have to be multiplexed with other peripherals. This
             * is not the case with Ethernet. Thus, this mode DOES NOT APPLY! */
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NOT_SUPPORTED);
            break;

        default:
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
            break;
        }
    }

    if (RETCODE_OK == rc)
    {
        mcuEth->EventCallback = callback;
        mcuEth->NextFrameOffset = 0U;
        mcuEth->RxPool = NULL;
        mcuEth->RxPoolLength = 0U;
        mcuEth->State = ETHERNET_STATE_INITIALIZED;
    }

    /* No need for cleanup in case of error. HAL_ETH_DMAXXDescListInit() does
     * only perform "soft" changes to the STM-HAL structure, which are hidden
     * to the user anyway. A re-init will overwrite them. */

    return rc;
}

Retcode_T MCU_Ethernet_StartReceive(Ethernet_T eth, const struct MCU_Ethernet_PhyisicalAddress_S *mac, struct MCU_Ethernet_FrameBuffer_S *rxPool, size_t rxPoolLength)
{
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;

    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_INITIALIZED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }
    else if (mcuEth->State & ETHERNET_STATE_RX_ONGOING)
    {
        /* Other transfer currently ongoing. User should wait for RxStopped
         * event! */
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    for (size_t i = 0; i < rxPoolLength; ++i)
    {
        if (NULL == rxPool->Data || 0U == rxPool->Size)
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        }
    }

    ETH_MACAddressConfig(&mcuEth->VendorHandle, ETH_MAC_ADDRESS0, mac->Mac);

    mcuEth->RxPool = rxPool;
    mcuEth->RxPoolLength = rxPoolLength;

    return mcuEth->StartReceive(mcuEth);
}

Retcode_T MCU_Ethernet_GetAvailableRxFrame(Ethernet_T eth, struct MCU_Ethernet_FrameBuffer_S **rxFrame)
{
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;

    if (rxFrame)
        *rxFrame = NULL;

    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_INITIALIZED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }

    HAL_StatusTypeDef halStat = HAL_ETH_GetReceivedFrame_IT(&mcuEth->VendorHandle);
    if (HAL_OK == halStat)
    {
        /* Find next free frame buffer from pool */
        struct MCU_Ethernet_FrameBuffer_S *freeFrame = NULL;
        for (size_t i = 0; i < mcuEth->RxPoolLength && NULL == freeFrame; ++i)
        {
            if (!mcuEth->RxPool[i].IsUserOwned)
                freeFrame = &mcuEth->RxPool[i];
        }

        /* Check if frame found and sufficiently big. */
        if (NULL == freeFrame && freeFrame->Size >= mcuEth->VendorHandle.RxFrameInfos.length)
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);

        /* Start copying data from the DMA buffers over to the user-space frame
         * buffer. */
        __IO ETH_DMADescTypeDef *descr = mcuEth->VendorHandle.RxFrameInfos.FSRxDesc;
        size_t offset = 0;
        while (offset < mcuEth->VendorHandle.RxFrameInfos.length)
        {
            assert(NULL != descr);
            assert(NULL != (void *)descr->Buffer1Addr);

            size_t numBytesLeft = mcuEth->VendorHandle.RxFrameInfos.length - offset;
            size_t numBytesToCopy = numBytesLeft > ETH_RX_BUF_SIZE ? ETH_RX_BUF_SIZE : numBytesLeft;

            memcpy((uint8_t *)freeFrame->Data + offset, (void *)descr->Buffer1Addr, numBytesToCopy);

            offset += numBytesToCopy;
            descr = (ETH_DMADescTypeDef *)descr->Buffer2NextDescAddr;
        }
        assert(offset == mcuEth->VendorHandle.RxFrameInfos.length);

        freeFrame->Length = offset;
        freeFrame->IsUserOwned = true;
        *rxFrame = freeFrame;

        /* We're done copying. We now return ownership of each descriptor back
         * to the DMA controller. */
        descr = mcuEth->VendorHandle.RxFrameInfos.FSRxDesc;
        for (uint32_t i = 0; i < mcuEth->VendorHandle.RxFrameInfos.SegCount; ++i)
        {
            descr->Status |= ETH_DMARXDESC_OWN;
            descr = (ETH_DMADescTypeDef *)descr->Buffer2NextDescAddr;
        }
        mcuEth->VendorHandle.RxFrameInfos.SegCount = 0;

        /* When "rx buffer unavailable"-bit set and rx-process is supposed to be
         * ongoing, clear the bits and issue a "receive poll demand" to resume
         * receive.
         *
         * Once the rx-DMA fills a chain element, that element is marked
         * CPU-owned until the ownership-bit is reset. If we fail to consume
         * these chain elements in time the DMA might starve, because if has no
         * free descriptor-buffers to put data in. In case that happens, DMA
         * will stop receiving and set the "rx buffer unavailable"-bit.
         *
         * Now that we have cleared some DMA chain elements, we should ask the
         * DMA to poll/check the chain elements again, and perhaps start
         * flushing any available bytes from the rx-FIFO. */
        if ((mcuEth->VendorHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET ||
            mcuEth->State & ETHERNET_STATE_RX_ONGOING)
        {
            /* Clear the "rx buffer unavailable" flag */
            mcuEth->VendorHandle.Instance->DMASR = ETH_DMASR_RBUS;

            /* Resume receive by writing some (really any) value into the
             * "Ethernet DMA receive poll demand" register. */
            mcuEth->VendorHandle.Instance->DMARPDR = 0;
        }

        return RETCODE_OK;
    }
    else if (HAL_ERROR == halStat)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MCU_ETHERNET_NODATA);
    }
    else if (HAL_BUSY == halStat)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
    else
    {
        return MapHalStatusToRetcode(halStat);
    }
}

Retcode_T MCU_Ethernet_ReturnToRxPool(Ethernet_T eth, struct MCU_Ethernet_FrameBuffer_S *rxFrameBuffer)
{
    KISO_UNUSED(eth);
    rxFrameBuffer->IsUserOwned = false;
    return RETCODE_OK;
}

Retcode_T MCU_Ethernet_StopReceive(Ethernet_T eth)
{
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;

    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_INITIALIZED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_RX_ONGOING))
    {
        /* No transfer ongoing. */
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    return mcuEth->StopReceive(mcuEth);
}

Retcode_T MCU_Ethernet_AppendToNextFrame(Ethernet_T eth, const void *data, size_t length)
{
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;
    __IO ETH_DMADescTypeDef *dmaTxDesc = mcuEth->VendorHandle.TxDesc;
    size_t bytesToCopy = length;

    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_INITIALIZED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }
    else if (mcuEth->NextFrameOffset + length > ETH_MAX_PACKET_SIZE)
    {
        /* Can't put that much data in a single frame. */
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
    }

    for (size_t i = 0; mcuEth->NextFrameOffset / ETH_TX_BUF_SIZE; ++i)
    {
        if (NULL == dmaTxDesc)
        {
            return RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_NULL_POINTER);
        }
        dmaTxDesc = (__IO ETH_DMADescTypeDef *)dmaTxDesc->Buffer2NextDescAddr;
    }
    size_t offset = mcuEth->NextFrameOffset % ETH_TX_BUF_SIZE;

    /* Depending on how the BSP allocated the TxBuffers for us, a single
     * TxBuffer may not be large enough the hold a full frame. That's OK! The
     * DMA will simply follow the descriptor-chain until it encounters one with
     * the "last segment"-bit set.
     *
     * This loop simply walks the chain and copies as many bytes as possible in
     * each descriptor-buffer. */
    while (NULL != dmaTxDesc && !(dmaTxDesc->Status & ETH_DMATXDESC_OWN) && bytesToCopy > 0)
    {
        size_t bytesCopied = bytesToCopy > ETH_TX_BUF_SIZE ? ETH_TX_BUF_SIZE : bytesToCopy;

        memcpy(((char *)dmaTxDesc->Buffer1Addr) + offset, data, bytesCopied);
        mcuEth->NextFrameOffset += bytesCopied;
        bytesToCopy -= bytesCopied;
        data = (const char *)data + bytesCopied;

        dmaTxDesc = (__IO ETH_DMADescTypeDef *)dmaTxDesc->Buffer2NextDescAddr;
        offset = 0;
    }

    /* Previous loop failed to find a place for each byte. */
    if (bytesToCopy > 0)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
    }
    else
    {
        return RETCODE_OK;
    }
}

Retcode_T MCU_Ethernet_ResetNextFrame(Ethernet_T eth)
{
    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;
    mcuEth->NextFrameOffset = 0U;
    /* No point in clearing each byte in the descriptor chain. Will be
     * overwritten with next few appends. */

    return RETCODE_OK;
}

Retcode_T MCU_Ethernet_TransmitFrame(Ethernet_T eth)
{
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;

    if (NULL == eth)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    else if (!(mcuEth->State & ETHERNET_STATE_INITIALIZED))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
    }
    else if (mcuEth->State & ETHERNET_STATE_TX_ONGOING)
    {
        /* Other transfer currently ongoing. User should wait for TxStopped
         * event! */
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }

    return mcuEth->TransmitFrame(mcuEth);
}

void MCU_Ethernet_Deinitialize(Ethernet_T eth)
{
    assert(eth);
    struct MCU_Ethernet_S *mcuEth = (struct MCU_Ethernet_S *)eth;
    if (mcuEth)
    {
        if (mcuEth->State & ETHERNET_STATE_TX_IDLE ||
            mcuEth->State & ETHERNET_STATE_TX_ONGOING)
        {
            CancelTransmit(mcuEth);
        }
        mcuEth->State = ETHERNET_STATE_UNINITIALIZED;
        mcuEth->EventCallback = NULL;
        mcuEth->TransmitFrame = NULL;
        mcuEth->StartReceive = NULL;
    }
}

#endif /* KISO_FEATURE_ETHERNET */
