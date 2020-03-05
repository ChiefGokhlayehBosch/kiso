/*******************************************************************************
 * Copyright (c) 2020 Robert Bosch GmbH
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
 * @defgroup KISO_HAL_MCU_ETHERNET MCU Ethernet
 * @{
 * @ingroup KISO_HAL_MCU_IF
 *
 * @brief Low-level driver for on-chip Ethernet MAC peripherals.
 *
 * This driver primarily handles the data-transfer portion of a given Ethernet
 * device. Hardware initialization is job of the Board Support Package (BSP),
 * which provides the user with a #HWHandle_T representing the Ethernet device
 * (see #Ethernet_T).
 *
 * Fed with such a Ethernet handle this API allows a user to send and receive
 * pre-formatted IEEE 802.3 3.1 frames (aka Ethernet II frames). Note that
 * the MCU API has no awareness of the contents of the Ethernet II frame. The
 * caller is expected to allocate a sufficiently sized buffer to hold the full
 * frame, including both source and destination address, as well as the ethernet
 * type. The frame check sequence (FCS) that's part of the trailer is handled
 * either by hardware or the MCU specific implementation. It is not part of the
 * allocated buffer. A typical frame buffer should therefore be able to hold at
 * least 1514 bytes (14 bytes header + 1500 payload).
 *
 * The underlying Ethernet peripheral is expected to implement a minimum set of
 * capabilities:
 * - Automatic generation of Ethernet FCSs (Frame Check Sequences) and appending
 * them to tx frames.
 * - Automatic checking and stripping of FCS on incoming frames.
 * - Filtering of incoming frames based on links' MAC-address.
 * - Automatic preamble and start-of-frame generation.
 * - Collision detection protocol (CSMA/CD).
 * - Inter-frame gap timout for consecutive transmits.
 * If these capabilities are not met by hardware, they may be emulated in
 * software.
 *
 * @startuml
 *
 *  title HAL MCU Ethernet
 *
 *  package Essentials {
 *      folder mcu {
 *          class HwHandle <<abstract>>
 *
 *          class MCU_Ethernet_Event {
 *              TxStopped : bool
 *              TxNextFrameReady : bool
 *              TxError : bool
 *              RxAvailable : bool
 *              RxStopped : bool
 *              RxError : bool
 *              DataLoss : bool
 *              IsIsr : bool
 *          }
 *
 *          class MCU_Ethernet_FrameBuffer {
 *              Data : ByteBuffer
 *              Length : UnsignedSize {Length<=Size}
 *              Size : UnsignedSize
 *              IsUserOwned : bool
 *          }
 *
 *          class MCU_Ethernet <<abstract>> {
 *              #TransferMode : HAL_TransferMode
 *              #NextFrameOffset : UnsignedSize
 *
 *              +{static} InitRxFrameBuffer(rxFragment : MCU_Ethernet_FrameBuffer, data : ByteBuffer, size : UnsignedSize) : void
 *
 *              +{abstract} <<ctor>>Initialize(callback : void (*)(eth : MCU_Ethernet, event : MCU_Ethernet_Event)) : Retcode
 *              +{abstract} <<dtor>>Deinitialize() : void
 *
 *              +{abstract} AppendToNextFrame(data : ByteBuffer, size : UnsignedSize) : Retcode
 *              +{abstract} ResetNextFrame() : Retcode
 *              +{abstract} TransmitFrame() : Retcode
 *              +{abstract} StopTransmit() : Retcode
 *              +{abstract} StartReceive(localMac : PhysicalAddress, rxPool : MCU_Ethernet_FrameBuffer[1..*]) : Retcode
 *              +{abstract} GetAvailableRxFrane() : MCU_Ethernet_FrameBuffer
 *              +{abstract} ReturnToRxPool(rxFrame : MCU_Ethernet_FrameBuffer) : Retcode_T
 *              +{abstract} StopReceive() : Retcode
 *          }
 *
 *          folder stm32f7 {
 *              class MCU_STM32F7_Ethernet {
 *                  -VendorHandle : ETH_Handle
 *                  -TxBufferDescriptors : ETH_DMADesc [NUM_TX_DESCR]
 *                  -RxBufferDescriptors : ETH_DMADesc [NUM_RX_DESCR]
 *              }
 *          }
 *      }
 *  }
 *
 *  HwHandle <|-- MCU_Ethernet
 *  MCU_Ethernet <|-- MCU_STM32F7_Ethernet
 *
 *  MCU_Ethernet . MCU_Ethernet_Event
 *
 *  MCU_Ethernet --> MCU_Ethernet_FrameBuffer : #RxPool
 *
 *  note bottom of MCU_Ethernet_FrameBuffer
 *  Each buffer should be large enough to hold a full frame.
 *  end note
 *
 *  footer Copyright (C) Robert Bosch GmbH
 *
 * @enduml
 *
 * @file
 * @brief Primary API header for sending and receiving Ethernet frames.
 */

#ifndef KISO_MCU_ETHERNET_H_
#define KISO_MCU_ETHERNET_H_

#include "Kiso_HAL.h"

/* Code is only effective if feature is enabled in configuration */
#if KISO_FEATURE_ETHERNET

#include "Kiso_Retcode.h"

/**
 * @brief Length of IEEE 802 MAC addresses in bytes.
 */
#define MCU_ETHERNET_MACLENGTH (6U)

/**
 * @brief Minimum total size of Ethernet II frames. Padding is inserted for
 * frames which content is less than #MCU_ETHERNET_MINFRAMESIZE.
 *
 * This excludes the trailer/FCS, adding another 4 bytes.
 */
#define MCU_ETHERNET_MINFRAMESIZE (60U)

/**
 * @brief Allows a generic hardware handle to be viewed as an Ethernet handle.
 * The #HWHandle_T is set up and provided by the BSP for use in this API.
 */
typedef HWHandle_T Ethernet_T;

/**
 * @brief Ethernet specific Retcode codes.
 */
enum Retcode_MCU_Ethernet_E
{
    RETCODE_MCU_ETHERNET_NODATA = RETCODE_FIRST_CUSTOM_CODE, //!< No data available.
};

/**
 * @brief Represents a phyisical address (MAC-address) of a Ethernet device
 */
struct MCU_Ethernet_PhyisicalAddress_S
{
    /**
     * @brief Individual bytes of the MAC address.
     *
     * The bytes are interpreted in order from left to right, low index to high
     * index. Example:
     * - `01:23:45:67:89:AB`
     *  - `mac[0] = 0x01`
     *  - `mac[1] = 0x23`
     *  - `mac[2] = 0x45`
     *  - `mac[3] = 0x67`
     *  - `mac[4] = 0x89`
     *  - `mac[5] = 0xAB`
     */
    uint8_t Mac[MCU_ETHERNET_MACLENGTH];
};

/**
 * @brief Represents a buffer to store received Ethernet frames in.
 *
 * The user is only expected to initialize the #Data and #Size fields with a
 * pre-allocated buffer and its associated size (see
 * #MCU_Ethernet_InitRxFrameBuffer()). Other fields like #IsUserOwned and
 * #Length are used later in the rx-process and may therefore be left
 * uninitialized by the user.
 *
 * The rx-process will use the #Data buffer as storage for incoming frames. The
 * buffer must be large enough to hold a full frame, including Ethernet header
 * and payload. The FCS is truncated and checked internally. It is not part of
 * the receivedframe. If the buffer is insufficiently sized, an error will be
 * indicated through the event callback.
 *
 * If the transfer of frame data from the Ethernet hardware into the user
 * provided buffer is completed successfully a
 * #MCU_Ethernet_Event_S::RxAvailable event will be thrown. Each frame will have
 * the #IsUserOwned flag set, indicating that the user may now safely consume
 * the data presented in the #Data buffer. Conversely, the MCU API is forbidden
 * from using a user-owned frame buffer in the rx-process. The user must
 * return ownership of the frame buffer back into the rx-pool
 * via #MCU_Ethernet_ReturnToRxPool().
 *
 * @warning Do no set #IsUserOwned directly from user-code. Always use
 * #MCU_Ethernet_ReturnToRxPool() if you intend to make a buffer available
 * to the rx-process again.
 */
struct MCU_Ethernet_FrameBuffer_S
{
    /**
     * @brief Data buffer used as storage during the rx-process.
     *
     * Must be allocated by the user.
     *
     * Must be able to hold the Ethernet header (`src` + `dst` + `type` = 14
     * bytes) and payload. Note that the buffer has a minimum total size of 64
     * bytes, as required by Ethernet II.
     *
     * @see #MCU_ETHERNET_MINFRAMESIZE
     *
     * @todo The buffer may be subject to DMA transfering and thus require
     * special start and size alignments, layouting as well as being allocated
     * in specific memory regions. This is an open point in the MCU API
     * buffer-handling and will be subject of change in the future. For new we
     * recommend users of this API to ensure their buffers are DMA'able (yes,
     * this does go against hardware-abstraction principles).
     */
    void *Data;

    /**
     * @brief Fill-level of the #Data buffer.
     *
     * Will be set by the MCU API internall once a frame is received and stored
     * inside this frame buffer.
     *
     * Will be ignored and reset for each frame in the rx-pool upon start of a
     * new rx-process.
     */
    size_t Length;

    /**
     * @brief Maximum allowed number of bytes allowed to be written into #Data.
     *
     * Must be set by the user.
     *
     * This does not indicate how many bytes were actually received, for that
     * see #Length.
     */
    size_t Size;

    /**
     * @brief If `true` marks that this frame buffers #Data and #Length fields
     * may safely be access by user-code.
     *
     * If a frame is marked user-owned, the rx-process is tasked with
     * excluding this frame from further receive processes from hw to memory.
     *
     * Will be ignored and reset for each frame in the rx-pool upon start of a
     * new rx-process.
     *
     * @see #MCU_Ethernet_ReturnToRxPool()
     */
    bool IsUserOwned;
};

/**
 * @brief Event structure passed to the user during hardware or API event.
 *
 * @note In some events multiple flags may be set at the same time to give more
 * hints about the underlying cause.
 */
struct MCU_Ethernet_Event_S
{
    /**
     * @brief Signals the end of all ongoing or cached frame transmissions.
     *
     * Indicates that the tx-frame-queue is empty and that no further data is
     * currently being transferred to the Ethernet peripheral.
     *
     * Does not necessarily imply successful transmission. Please check #TxError
     * and #DataLoss as well.
     *
     * Users may want to listen for this event when preparing to shut down
     * communication.
     */
    uint32_t TxStopped : 1;

    /**
     * @brief Indicates that a user may proceed with preparing the next frame
     * for transmission.
     *
     * This does @b not indicate that the previous frame was fully transferred
     * to the Ethernet peripheral (see #TxStopped for that). In fact, the
     * previous frame may haven't even been completely transferred yet. Still,
     * the user may already start with preparing the next one, to allow higher
     * throughput.
     *
     * In implementations that only allow a single frame to be transferred at a
     * time, this event may always combine with #TxStopped.
     *
     * If previously #TxStopped was indicated, a user may still have to wait
     * until #TxNextFrameReady is signalled before calling
     * #MCU_Ethernet_AppendToNextFrame() and #MCU_Ethernet_TransmitFrame().
     */
    uint32_t TxNextFrameReady : 1;

    /**
     * @brief Transmit error has occurred.
     *
     * Could indicate an underlying hardware fault or an error encountered in
     * the device specific implication.
     *
     * If due to this error a tx-frame has been lost #DataLoss will be set too.
     */
    uint32_t TxError : 1;

    /**
     * @brief At least one frame contains data.
     *
     * Check the #MCU_Ethernet_FrameBuffer_S::IsUserOwned flag of the buffers
     * supplied to #MCU_Ethernet_StartReceive().
     */
    uint32_t RxAvailable : 1;

    /**
     * @brief Rx-process stopped.
     *
     * If the rx-process stops, the following events might have occurred.
     * - The user request a stop via #MCU_Ethernet_StopReceive().
     * - A hardware error occurred during receive process which hinders the
     * rx-process form continueing.
     * - The #MCU_Ethernet_FrameBuffer_S supplied during
     * #MCU_Ethernet_StartReceive() was not circularly linked. In such a case the
     * rx-process gracefully stops after receiving and storing the last frame
     * in the sequence. In this case #DataLoss will also be set.
     *
     * In any case, the receive process may be restarted/retried via call to
     * #MCU_Ethernet_StartReceive().
     */
    uint32_t RxStopped : 1;

    /**
     * @brief Receive error has occurred.
     *
     * This may indicate the following issues:
     * - An underlying hardware fault was encountered.
     * - One of the #MCU_Ethernet_FrameBuffer_S provided during
     * #MCU_Ethernet_StartReceive() had insufficient space to store the full frame.
     * - The device specific implementation encountered an unexpected error
     * during the receive process.
     */
    uint32_t RxError : 1;

    /**
     * @brief Indicates the additional loss of data due to #RxError or #TxError.
     *
     * A loss of data always implies an error.
     */
    uint32_t DataLoss : 1;

    /**
     * @brief Indicates if this callback is executed in interrupt context.
     *
     * Which events are called from ISR and which are not is up to the
     * implementation. A user should therefore always check the #IsIsr flag and
     * change their control-flow accordingly.
     */
    uint32_t IsIsr : 1;
};
static_assert(sizeof(struct MCU_Ethernet_Event_S) <= sizeof(uint32_t), "Because the events are passed around as call-by-value, the bitflags inside MCU_Ethernet_Event_S should fit in one word.");

/**
 * @brief Callback to be supplied during MCU peripheral initialization. Called
 * by the API to inform about various events.
 *
 * @note Expect this callback to be invoked in ISR context.
 *
 * @param[in] eth
 * Ethernet device the event occurred on.
 *
 * @param[in] event
 * The event structure to be processed by the callback.
 */
typedef void (*MCU_Ethernet_EventCallback_T)(Ethernet_T eth, struct MCU_Ethernet_Event_S event);

/**
 * @brief Initialize the given Ethernet device and register a callback
 * for device events..
 *
 * @param eth
 * Handle of the Ethernet device to be initialized. The handle is typically
 * provided by the @ref KISO_HAL_BSP_IF API.
 *
 * @param[in] callback
 * Callback function to be registered for this Ethernet device. Will be called
 * to communicate various device events.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_Initialize(Ethernet_T eth, MCU_Ethernet_EventCallback_T callback);

/**
 * @brief Start enabling the reception of frames from the Ethernet device and
 * store them in the provided rx-pool.
 *
 * The rx-process will take ownership of the provided rx-pool and try to use it
 * to store incoming frames. If the provided pool can not be used by hardware
 * directly, a "fallback" shall be implemented.
 *
 * For each received frame a #MCU_Ethernet_Event_S::RxAvailable event is
 * triggered once. The user may then use #MCU_Ethernet_GetAvailableRxFrame() to
 * obtain a pointer to the new frame. #MCU_Ethernet_FrameBuffer_S::Length
 * field will be set to the amount of bytes available in
 * #MCU_Ethernet_FrameBuffer_S::Data. Additionally, the frame will be
 * temporarily taken out of the rx-pool (indicated by
 * #MCU_Ethernet_FrameBuffer_S::IsUserOwned flag). Note that rx-process is
 * forbidden from using a #MCU_Ethernet_FrameBuffer_S until it is returned to
 * the rx-pool via #MCU_Ethernet_ReturnToRxPool().
 *
 * The rx-process is continuous and runs indefinitely until stopped by either a
 * fatal hardware error or user request (see #MCU_Ethernet_StopReceive()).
 *
 * Incoming frames are filtered by destination MAC address. Only matching
 * frames are forwarded to the user. Mismatching frames are dropped.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @param[in] mac
 * MAC address of the local link used for filtering incoming frames.
 *
 * @param[in,out] rxPool
 * Array of frame buffers used as storage during the rx-process.
 *
 * @param[in] rxPoolLength
 * Length of the @p rxPool array.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_StartReceive(Ethernet_T eth, const struct MCU_Ethernet_PhyisicalAddress_S *mac, struct MCU_Ethernet_FrameBuffer_S *rxPool, size_t rxPoolLength);

/**
 * @brief Look through the rx-pool and provide a pointer to a frame containing
 * data received from the Ethernet peripheral.
 *
 * The returned @p rxFrame is guaranteed to be user-owned.
 *
 * To avoid polling, listen for the #MCU_Ethernet_Event_S::RxAvailable event.
 *
 * If multiple frames are available, the function will always return the same
 * user-owned frame buffer until that one is returned to the pool via
 * #MCU_Ethernet_ReturnToRxPool().
 *
 * The order in which available frames are returned is arbitrary. A user should
 * not expect the function to return frames in order of receiption.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @param[out] rxFrame
 * On success, will be set to one of the frames rx-pool containing data. Set to
 * `NULL` if no frame containing rx data can be found.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 * @retval RETCODE_OK
 * A suitable frame was found and it's address was written to `*rxFrame`.
 * @retval RETCODE_MCU_ETHERNET_NODATA
 * No suitable frame was found in the rx-pool and @p *rxFrame is set to `NULL`.
 * Indicates that no frames were received.
 */
Retcode_T MCU_Ethernet_GetAvailableRxFrame(Ethernet_T eth, struct MCU_Ethernet_FrameBuffer_S **rxFrame);

/**
 * @brief Return the given user-owned rx frame buffer back into the rx-pool.
 *
 * By transfering the frame buffer back into the rx-pool, ownership is returned
 * to the rx-process. Accessing the frame buffer while ownership is given to the
 * rx-process may result in undefined behavior.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @param[in,out] rxFrameBuffer
 * Frame buffer to return to the rx-pool.
 *
 * Must be user-owned and part of the original rx-pool initiated by
 * #MCU_Ethernet_StartReceive(), otherwise an error is returned.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_ReturnToRxPool(Ethernet_T eth, struct MCU_Ethernet_FrameBuffer_S *rxFrameBuffer);

/**
 * @brief Stop an ongoing rx-process.
 *
 * The stop is asynchronous, meaning the rx-process is not guaranteed to have
 * completely stopped upon function return. A #MCU_Ethernet_Event_S::RxStopped
 * event will be triggered once the rx-process is considered stopped.
 *
 * If the rx-process is already stopped, no action is performed and no error is
 * returned.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_StopReceive(Ethernet_T eth);

/**
 * @brief Appends bytes to the next-in-queue tx-frame.
 *
 * Preamble, start-of-frame and FCS are generated internally and must not be
 * part of the constructed tx frame buffer.
 *
 * The frame is not sent out until #MCU_Ethernet_TransmitFrame() is called. A
 * user may call for append multiple times to piece together a full frame.
 *
 * Data is copied, so the @p data buffer may be used otherwise after function
 * exit.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @param[in] data
 * Data buffer to be copied over in the to-be-prepared frame.
 *
 * @param[in] length
 * Length of @p data in bytes.
 *
 * @param[in] offset
 * Byte offset within the to-be-prepared frame.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_AppendToNextFrame(Ethernet_T eth, const void *data, size_t length);

/**
 * @brief Reset the state of the next-in-queue frame.
 *
 * Clears the append-offset at which #MCU_Ethernet_AppendToNextFrame() inserts
 * bytes.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_ResetNextFrame(Ethernet_T eth);

/**
 * @brief Transfer a single frame over the Ethernet peripheral.
 *
 * The frame must be preconstructed by the user via
 * #MCU_Ethernet_AppendToNextFrame().
 *
 * The transmit process is asynchronous. Upon successful transfer over the
 * Ethernet peripheral a #MCU_Ethernet_Event_S::TxStopped event is triggered.
 * In cases where the implementation is capable of providing a next frame buffer
 * slot before the previous one has been completely transferred,
 * #MCU_Ethernet_Event_S::TxNextFrameReady may be indicated before
 * #MCU_Ethernet_Event_S::TxStopped.
 *
 * The hardware may perform collision detection and automatic retransmissions to
 * the best of its capabilities. If a collision is detected and the frame could
 * not be transferred even after the hardware-provided retry mechanism a
 * #MCU_Ethernet_Event_S::TxError is triggered.
 *
 * If the Ethernet hardware is not full-duplex capable, an ongoing rx-process
 * may get temporarily stopped and automatically resumed upon transmit
 * complete/error. This is not considered a true receive stop, therefore no
 * #MCU_Ethernet_Event_S::RxStopped event will be triggered. A user should note
 * that depending on the hardware, they may temporarily turn deaf while sending
 * a frame.
 *
 * @param eth
 * Handle of the Ethernet device to work with.
 *
 * @return A #Retcode_T indicating the result of the requested action.
 */
Retcode_T MCU_Ethernet_TransmitFrame(Ethernet_T eth);

/**
 * @brief Deinitialize the Ethernet device associated with the given handle.
 *
 * @param eth
 * Handle to deinitialize.
 */
void MCU_Ethernet_Deinitialize(Ethernet_T eth);

/**
 * @brief Utility function to initialize the user-controllable fields of a
 * #MCU_Ethernet_FrameBuffer_S.
 *
 * The initialized buffer is afterwards ready to be used in the rx-process (see
 * #MCU_Ethernet_StartReceive()).
 *
 * @param[in,out] rxFrameBuf
 * The Frame to be initialized.
 *
 * @param[in,out] data
 * Data buffer to be associated with the given @p rxFrameBuf.
 *
 * @param[in] size
 * Size of @p data in bytes.
 */
inline void MCU_Ethernet_InitRxFrameBuffer(struct MCU_Ethernet_FrameBuffer_S *rxFrameBuf, void *data, size_t size)
{
    rxFrameBuf->Data = data;
    rxFrameBuf->Size = size;
}

#endif /* KISO_FEATURE_ETHERNET */
#endif /* KISO_MCU_ETHERNET_H_ */

/** @}*/