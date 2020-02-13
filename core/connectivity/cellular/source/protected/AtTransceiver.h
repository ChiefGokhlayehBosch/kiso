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

/**
 * @ingroup KISO_CELLULAR_COMMON
 *
 * @defgroup ATTRANSCEIVER AT Transceiver
 * @{
 *
 * @brief Transceiver designed to interact with an AT capable modem interface.
 *
 * @details AT (short for "ATTENTION"), is a textual protocol used to interact
 * with modems of various classes (cellular, WLAN, DSL, to name a few). The
 * typical AT setup involves a "Terminal Adaptor" (TA) and "Terminal Equipment"
 * (TE). The TA is usually part of the physical modem package/SoC and provides
 * the AT interface for interacting with the "Data Terminal Equipment" (DCE),
 * i.e. the part of the modem handling the network. TE refers to the component
 * that issues AT commands to the TA (for example: initiate dial-up, send SMS,
 * instantziate TCP/IP protocol stack, etc.). In exchange, the TA sends out AT
 * responses to the TE, which depending on the command contains network status
 * information, payload or other information.
 * This transceiver implementation is designed as lower-level part of the TE.
 * Because AT covers such a wide range of devices and use-cases, implementing
 * the TE is best split into multiple components, including:
 *  - Tokenizing: Splitting an incoming character stream containing AT
 * responses into chunks according to the standardized AT syntax. The actual
 * content of these chunks is at first ignored and passed to an Interpreter.
 *  - Interpreting: Inspect the chunks received from the Tokenizer and,
 * utilizing knowledge of the modem at hand, act on the data received.
 *  - Command-building: Formulating AT commands, following AT syntax, and
 * sending them to the TA. This also includes associating the subsequent
 * response to the right part of the Interpreter.
 *
 * This module implements the "Tokenizer" (Rx) and "Command-builder" (Tx)
 * described above. It does so by implementing a "smart ring-buffer" design.
 * A module integrator can feed bytes coming from the TA into the transceiver.
 * The fed bytes are stored in a ring-buffer, until an user calls the
 * `AtTransceiver_Read...()` function-set (referred to as read API). To send out
 * commands, the transceiver implements a command-builder interface. The
 * `AtTransceiver_Write...()` function-set (referred to as write API) is used
 * for this purpose.
 *
 * AT typically identifies two distinct types of AT responses:
 *  - solicited and
 *  - unsolicited
 *
 * Solicited responses are the TAs' answer to a specific AT command. This
 * response may arrive with several seconds delay (as a result of a long runing
 * action), during which no AT communication in either direction is recommended.
 * Hence the TA can only handle one concurrent action at a time. Some TAs
 * implement an @b abort feature, which is typically invoked by sending
 * arbitrary bytes during an ongoing action.
 *
 * Unsolicited responses (often referred to as URC, Unsolicited Response Code)
 * are the TAs' way of notifying about random events not associated to any
 * previous AT command (such as incoming call, packet data arrival, or network
 * disconnect). This transceiver operates under the assumption, that solicited
 * and unsolicited responses are strictly sequentialized. Meaning, a solicited
 * response will not be interrupted or preempted by an unsolicited response and
 * vice versa. TAs typically buffer unsolicited responses until a "moment of
 * silence" (no traffic on the interface), in which the unsolicited responses
 * are flushed. Hardware/software flow-control may help the TA find such a
 * moment and prevent collisions. Note that any form of flow-control should be
 * implemented _below_ the AtTransceiver and is hence not part of this
 * implementation.
 *
 * To facilitate the distinction between solicited and unsolicited responses,
 * the transceiver implements a locking behaviour. This guarantees that a
 * command-sender receives the associated response without being preempted by a
 * URC-listener. Conversely, an URC-listener continuously tries to obtain the
 * transceivers' lock. If the lock is obtained, i.e. no command is currently
 * ongoing, the listener may interpret any data available as URC and returns the
 * lock on completion.
 *
 * The Interpreter working with this transceiver is expected to have knowledge
 * of the AT command/response structure. When reading, the Interpreter instructs
 * the transceiver to read the next couple of bytes as "command", "argument", or
 * "response code". Hence the transceiver itself, does not enforce any structure
 * on the response. Only the Interpreter is capable of knowing what type of
 * token comes next. This design is enabled by ATs' sequential nature, i.e. an
 * AT response always begins with a command mnemonic, based on which the
 * Interpreter can know which and how many arguments may follow. An argument
 * may also carry information on the nature of subsequent arguments within the
 * same response, meaning the Interpreter is recommended to inspect command
 * mnemonic, and each argument sequentially and adapt accordingly.
 *
 * Find below some example usages. Note that for the sake of clarity and brevity
 * the examples do no perform any error handling. A implementor should of course
 * always respect the error codes and act accordingly.
 *
 * @par Initialization
 * @code{.c}
 *  struct AtTransceiver_S transceiver;
 *  uint8_t rxBuffer[1024];
 *
 *  Retcode_T WriteToUart(const void *data, size_t length, size_t *numBytesWritten)
 *  {
 *      // write bytes to UART
 *      if (numBytesWritten)
 *          *numBytesWritten = length;
 *      return RETCODE_OK;
 *  }
 *
 *  void main(void)
 *  {
 *      AtTransceiver_Initialize(&transceiver, rxBuffer, sizeof(rxBuffer), WriteToUart);
 *
 *      // do stuff with transceiver
 *  }
 * @endcode
 *
 * @par Feeding bytes into the transceiver
 * @code{.c}
 *  // initialization omitted
 *  struct AtTransceiver_S transceiver;
 *  void DataAvailableOnUartIsr(const void *data, size_t length)
 *  {
 *      // note: numActualBytesFed can be set to NULL if the caller is not
 *      // interested in a value
 *      AtTransceiver_Feed(&transceiver, data, length, NULL);
 *  }
 *
 *  void RunParserTask(void)
 *  {
 *      AtTransceiver_Lock(&transceiver);
 *
 *      uint8_t buffer[1024];
 *      size_t lengthOfBuffer = 0;
 *      AtTransceiver_Read(&transceiver, buffer, sizeof(buffer), &lengthOfBuffer, 1000);
 *
 *      for (size_t i = 0; i < lengthOfBuffer; ++i)
 *      {
 *          // work with data in buffer
 *      }
 *
 *      AtTransceiver_Unlock(&transceiver);
 *  }
 * @endcode
 *
 * @file
 */

#ifndef ATTRANSCEIVER_H_
#define ATTRANSCEIVER_H_

#include "Kiso_Basics.h"
#include "Kiso_Retcode.h"

#include "Kiso_RingBuffer.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "portmacro.h"

/**
 * @brief Represents octal (8) radix. Used for reading or writing integer
 * in octal notation.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #AtTransceiver_WriteI16()
 * @see #AtTransceiver_WriteI32()
 * @see #AtTransceiver_WriteU8()
 * @see #AtTransceiver_WriteU16()
 * @see #AtTransceiver_WriteU32()
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadU32()
 */
#define ATTRANSCEIVER_OCTAL (8)

/**
 * @brief Represents decimal (10) radix. Used for reading or writing integer
 * in decimal notation.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #AtTransceiver_WriteI16()
 * @see #AtTransceiver_WriteI32()
 * @see #AtTransceiver_WriteU8()
 * @see #AtTransceiver_WriteU16()
 * @see #AtTransceiver_WriteU32()
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadU32()
 */
#define ATTRANSCEIVER_DECIMAL (10)

/**
 * @brief Represents hexadecimal (16) radix. Used for reading or writing integer
 * in hexadecimal notation.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #AtTransceiver_WriteI16()
 * @see #AtTransceiver_WriteI32()
 * @see #AtTransceiver_WriteU8()
 * @see #AtTransceiver_WriteU16()
 * @see #AtTransceiver_WriteU32()
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadU32()
 */
#define ATTRANSCEIVER_HEXADECIMAL (16)

/**
 * @brief Additional options which may be passed to
 * #AtTransceiver_PrepareWrite() to affect the write-sequences' behavior.
 * Options can be bit-OR'ed to combine effects.
 */
enum AtTransceiver_WriteOption_E
{
    ATTRANSCEIVER_WRITEOPTION_NONE = 0,             //!< No special write options (default behavior).
    ATTRANSCEIVER_WRITEOPTION_NOECHO = 1 << 0,      //!< Do not wait for an echo of the sent command during flush.
    ATTRANSCEIVER_WRITEOPTION_NOFINALS3S4 = 1 << 1, //!< Do not finish the command with `<S3><S4>` characters during flush.
    ATTRANSCEIVER_WRITEOPTION_NOSTATE = 1 << 2,     //!< Do not check or maintain state information during this write. Useful for passthrough operation mode.
    ATTRANSCEIVER_WRITEOPTION_NOBUFFER = 1 << 3     //!< Do not use the write buffer and instead, pass every write call directly to the write callback function.
};

/**
 * @brief Default write options to be used with #AtTransceiver_PrepareWrite() if
 * no special behaviour is requested.
 */
#define ATTRANSCEIVER_WRITEOPTION_DEFAULT (ATTRANSCEIVER_WRITEOPTION_NONE)

/**
 * @brief Representations of the internal write state-machine used for
 * constructing and validation of syntactically correct AT commands.
 *
 * The following shows separate state progressions of action, set and get
 * commands respectively. Note: State #ATTRANSCEIVER_WRITESTATE_INVALID is not
 * shown and can be assumed unused unless write option
 * #ATTRANSCEIVER_WRITEOPTION_NOSTATE is used.
 *
 * @par Get command `AT+COPS?`
 * @startuml
 *  [*] --> [*] : 1. '<b>AT+COPS?</b>' written, command marked complete
 * @enduml
 *
 * @par Set command `AT+COPS=1,1`
 * @startuml
 *  [*] --> COMMAND : 1. '<b>AT+COPS=</b>' written
 *  COMMAND -> ARGUMENT : 2. 'AT+COPS=<b>1</b>' written
 *  ARGUMENT -> ARGUMENT : 3. 'AT+COPS=1<b>,1</b>' written
 *  ARGUMENT --> [*] : 4. command marked complete
 * @enduml
 *
 * @par Action command `ATE1`
 * @startuml
 *  [*] --> [*] : 1. '<b>ATE1</b>' written, command marked complete
 * @enduml
 */
enum AtTransceiver_WriteState_E
{
    ATTRANSCEIVER_WRITESTATE_INVALID = 0,       //!< Invalid write state, or not applicable (depending on context).
    ATTRANSCEIVER_WRITESTATE_START = 1 << 0,    //!< Write state at the start of a fresh AT command (no bytes written).
    ATTRANSCEIVER_WRITESTATE_COMMAND = 1 << 1,  //!< Write state after command string was written (applicable to 'set' commands only).
    ATTRANSCEIVER_WRITESTATE_ARGUMENT = 1 << 2, //!< Write state after one or more arguments were written (applicable to 'set' commands only).
    ATTRANSCEIVER_WRITESTATE_END = 1 << 3       //!< AT command semantically complete. Final `<S3><S4>` may be written by calling #AtTransceiver_Flush().
};

/**
 * @brief Enum representation of final, intermediate and unsolicited AT response
 * codes.
 */
enum AtTransceiver_ResponseCode_E
{
    ATTRANSCEIVER_RESPONSECODE_OK = 0,         //!< Represents final AT response code `OK`.
    ATTRANSCEIVER_RESPONSECODE_CONNECT,        //!< Represents intermediate AT response code `CONNECT`.
    ATTRANSCEIVER_RESPONSECODE_RING,           //!< Represents unsolicited AT response code `RING`.
    ATTRANSCEIVER_RESPONSECODE_NOCARRIER,      //!< Represents final AT response code `NO CARRIER`.
    ATTRANSCEIVER_RESPONSECODE_ERROR,          //!< Represents final AT response code `ERROR`.
    ATTRANSCEIVER_RESPONSECODE_NODIALTONE,     //!< Represents final AT response code `NO DIALTONE`.
    ATTRANSCEIVER_RESPONSECODE_BUSY,           //!< Represents final AT response code `BUSY`.
    ATTRANSCEIVER_RESPONSECODE_NOANSWER,       //!< Represents final AT response code `NO ANSWER`.
    ATTRANSCEIVER_RESPONSECODE_CONNECTDR,      //!< Represents intermediate AT response code `CONNECT<data-rate>`.
    ATTRANSCEIVER_RESPONSECODE_NOTSUPPORTED,   //!< Represents final AT response code `NO ANSWER`.
    ATTRANSCEIVER_RESPONSECODE_INVALIDCMDLINE, //!< Represents final AT response code `INVALID COMMAND LINE`.
    ATTRANSCEIVER_RESPONSECODE_CR,             //!< Represents final AT response code `CR`.
    ATTRANSCEIVER_RESPONSECODE_SIMDROP,        //!< Represents final AT response code `SIM DROP`.
    ATTRANSCEIVER_RESPONSECODE_SENDOK,         //!< Represents final AT response code `SEND OK`.
    ATTRANSCEIVER_RESPONSECODE_SENDFAIL,       //!< Represents final AT response code `SEND FAIL`.
    ATTRANSCEIVER_RESPONSECODE_ABORTED,        //!< Represents final AT response code `ABORTED`.

    ATTRANSCEIVER_RESPONSECODE_MAX //!< Invalid response code. Used only for iterating the enum.
};

typedef Retcode_T (*AtTransceiver_WriteFunction_T)(const void *data, size_t length, size_t *numBytesWritten);

/**
 * @brief Represents a AT transceiver object. Holds internal state information
 * needed by the transceiver API.
 *
 * @note The layout and size of this struct may change without notice. Do not
 * access attributes directly.
 */
struct AtTransceiver_S
{
    /**
     * @brief Ring-buffer used to access received bytes.
     */
    RingBuffer_T RxRingBuffer;

    /**
     * @brief Indicates that the transceiver's read-end currently sits at the
     * start of a new line.
     *
     * The primary purpose of this attribute it to let the interpeter know if
     * the last argument they read was the final or an intermediate one. Thus
     * allowing the interpreter to work with dynamic responses of unkown
     * argument count.
     *
     * This attribute is only updated by syntax-aware read functions such as
     * #AtTransceiver_ReadString(). It is not updated by #AtTransceiver_Read().
     *
     * Users should use #AtTransceiver_IsStartOfLine() instead of
     * reading the value directly from the structure.
     */
    bool StartOfLine;

    /**
     * @brief Pointer to the Tx-buffer used for constructing a AT command.
     *
     * When referenced, interpreted as byte-buffer. Only valid during ongoing
     * write-sequence -- initiated via #AtTransceiver_PrepareWrite() and
     * completed via #AtTransceiver_Flush().
     */
    void *TxBuffer;

    /**
     * @brief Number of bytes in use, or sent out.
     *
     * Represents the number of bytes currently stored in TxBuffer, or, in case
     * #ATTRANSCEIVER_WRITEOPTION_NOBUFFER is set, number of bytes written over
     * the course of the current write-sequence.
     */
    size_t TxBufferUsed;

    /**
     * @brief Allocated size of #TxBuffer in bytes. Marks the maximum amount of
     * bytes able to be written into #TxBuffer.
     */
    size_t TxBufferLength;

    /**
     * @brief Write options associated with the ongoing write-sequence. Should
     * otherwise be ignored. Set by #AtTransceiver_PrepareWrite().
     */
    enum AtTransceiver_WriteOption_E WriteOptions;

    /**
     * @brief Current write state of the ongoing write-sequence. Should
     * otherwise be ignored.
     */
    enum AtTransceiver_WriteState_E WriteState;

    /**
     * @brief Write callback tasked with send bytes to the modem.
     */
    AtTransceiver_WriteFunction_T Write;

    /**
     * @brief Semaphore control-block needed for #RxWakeupHandle.
     */
    StaticSemaphore_t RxWakeupBuffer;

    /**
     * @brief Semaphore used to signal the arival of bytes, fed in to the
     * transceiver via #AtTransceiver_Feed().
     */
    SemaphoreHandle_t RxWakeupHandle;

    /**
     * @brief Semaphore control-block needed for #LockHandle.
     */
    StaticSemaphore_t LockBuffer;

    /**
     * @brief Handle for locking the transceiver.
     */
    SemaphoreHandle_t LockHandle;
};

/**
 * @brief Initialize a new AT transceiver instance.
 *
 * The @p rxBuffer is used for storing the raw AT response traffic. Bytes are
 * fed into the transceiver via #AtTransceiver_Feed(). They can then be consumed
 * as AT tokens through the transceivers' read API. The size of @p rxBuffer
 * should be chosen such that latencies between feeding and consuming can be
 * sufficiently bridged. The size also represents the maximum allowed token
 * length that may be obtained through the read API.
 *
 * @param[out] t
 * The transceiver to be initialized.
 * @param[out] rxBuffer
 * Receive buffer to associated with this transceiver. While assigned, the
 * transceiver require full ownership of the data pointed to. No other factors
 * are allowed to access or change contents of this buffer while it is in use.
 * Ownership is returned only through deinitialization of the transceiver. Will
 * be interpreted as byte buffer.
 * @param[in] rxLength
 * Length of @p rxBuffer in bytes.
 * @param[in] writeFunc
 * Function to be called when the transceiver intends to send an AT command
 * buffer to the cellular modem.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Initialize(struct AtTransceiver_S *t, void *rxBuffer, size_t rxLength, AtTransceiver_WriteFunction_T writeFunc);

/**
 * @brief Lock the transceiver instance from concurrent access.
 *
 * This must be done to protect consecutive calls to the read/write API. If the
 * transceiver is already locked by a different thread, the calling thread will
 * be blocked until the thread holding the lock releases it again.
 *
 * @param[in,out] t
 * Transceiver instance to be locked.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Lock(struct AtTransceiver_S *t);

/**
 * @brief Try to lock the transceiver instance from concurrent access.
 *
 * This must be done to protect consecutive calls to the read/write API. If the
 * transceiver is already locked by a different thread, the calling thread will
 * wait for the lock to be released within the specified timeout. If the lock
 * remains unavailable after the timeout is exceeded the function returns with
 * an error.
 *
 * @param[in,out] t
 * Transceiver instance to be locked.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see AtTransceiver_Lock
 */
Retcode_T AtTransceiver_TryLock(struct AtTransceiver_S *t, TickType_t timeout);

/**
 * @brief Prepare a transceiver for writing a command string.
 *
 * This will discard the contents of any previous writes and set options for the
 * subsequent write-sequence.
 *
 * A caller can specify the write option #ATTRANSCEIVER_WRITEOPTION_NOBUFFER, in
 * which case the arguments for @p txBuffer and @p txLength are ignored (and may
 * hence be set to `NULL` or `0` respectively). In this mode, any call the the
 * write API will immediately flush the generated string to the transceivers'
 * #AtTransceiver_S::Write. This string may only be a piece of the total AT
 * command string. The transceiver integrator must therefore take appropriate
 * action to buffer and combine the incoming command pieces into a consecutive
 * byte-stream. This, of course, only affects interfaces which rely on the AT
 * commands arriving as consecutive strings. Terminal Adaptors (TA) connected
 * via serial interface, like UART, typically implement their own buffering and
 * hence don't require buffer- and combineing.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @note If #ATTRANSCEIVER_WRITEOPTION_NOBUFFER is passed as option, the
 * transceiver will be unable to perform any echo verification. It will,
 * however, ensure that the number of bytes expected from the echo are consumed
 * upon flush.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perform the action on.
 * @param[in] options
 * Options to be passed to this write-sequence. Will have an impact on all
 * subsequent write calls including flush, until #AtTransceiver_PrepareWrite()
 * is called again.
 * @param[out] txBuffer
 * Buffer to store the outgoing AT command being constructed during this
 * write-sequence. The caller must ensure that the buffer remains allocated
 * during the entire write-sequence, until flush is completed. Afterwards the
 * buffer may be re-used for a subsequent write-sequence. Do not modify the
 * contents of @p txBuffer during the write-sequence, except via the provided
 * write API. The provided buffer must be large enough to hold the fully
 * constructed AT command including zero-termination. If
 * #ATTRANSCEIVER_WRITEOPTION_NOBUFFER is set, this paramter will be ignored and
 * may be set to `NULL`. Will be interpreted as byte buffer.
 * @param[in] txLength
 * Length of @p txBuffer in bytes. Will be ignored if
 * #ATTRANSCEIVER_WRITEOPTION_NOBUFFER is set.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_PrepareWrite(struct AtTransceiver_S *t, enum AtTransceiver_WriteOption_E options, void *txBuffer, size_t txLength);

/**
 * @brief Write an AT action command in the form `AT<action><S3><S4>`.
 *
 * An AT action command typically does not support parameters. Any subsequent
 * calls to add parameters will therefore be ignored and return an error, unless
 * #ATTRANSCEIVER_WRITEOPTION_NOSTATE is set.
 *
 * The `AT` characters will be inserted by the API and may not be part of the
 * action string. The caller must also provide a potential command prefix (like
 * '+') if applicable.
 * Examples:
 * 1. AT command `"ATE<S3><S4>"`, which enables echo mode on ITU-T V.250 conform
 * modems, has no prefix. The action parameter therefore simply consists of:
 * `"E"` (note: `"AT"` and `"<S3><S4>"` will be inserted by the API).
 * 2. AT command `"AT+CGMM<S3><S4>"`, which prints the model identification. The
 * action parameter for this command contains a prefix and command mnemonic:
 * `"+CGMM"`.
 * 3. AT command `"AT<S3><S4>",` which is used to check AT interface
 * availability, is an empty action. The action parameter must therefore be an
 * empty string `""` (not `NULL`!).
 *
 * After successful completion, the tx buffer may contain the following:
 * ```plain
 * AT<action>|
 *           ^ new write-courser
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] action
 * A c-string containing the action command mnemonic including potential prefix.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_PrepareWrite()
 * @see #AtTransceiver_WriteSet()
 * @see #AtTransceiver_WriteGet()
 */
Retcode_T AtTransceiver_WriteAction(struct AtTransceiver_S *t, const char *action);

/**
 * @brief Write an AT set command in the form `AT<set>=<...><S3><S4>`.
 *
 * An AT set command may be followed-up by additional AT parameters. This can be
 * done through the other write functions.
 *
 * The `AT` and `=` characters will be inserted by the API and may not be part
 * of the set string.
 *
 * After successful completion, the tx buffer may contain the following:
 * ```plain
 * AT<set>=|
 *         ^ new write-courser
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] set
 * A c-string containing the set command mnemonic including potential prefix.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_PrepareWrite()
 * @see #AtTransceiver_WriteAction()
 * @see #AtTransceiver_WriteGet()
 */
Retcode_T AtTransceiver_WriteSet(struct AtTransceiver_S *t, const char *set);

/**
 * @brief Write an AT get command in the form `AT<get>?<S3><S4>`.
 *
 * An AT get command typically does not support parameters. Any subsequent calls
 * to add parameters will therefore be ignored and return an error, unless
 * #ATTRANSCEIVER_WRITEOPTION_NOSTATE is set.
 *
 * The `AT` and `?` characters will be inserted by the API and may not be part
 * of the get string.
 *
 * After successful completion, the tx buffer may contain the following:
 * ```plain
 * AT<set>?|
 *         ^ new write-courser
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] get
 * A c-string containing the get command mnemonic including potential prefix.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_PrepareWrite()
 * @see #AtTransceiver_WriteAction()
 * @see #AtTransceiver_WriteSet()
 */
Retcode_T AtTransceiver_WriteGet(struct AtTransceiver_S *t, const char *get);

/**
 * @brief Write a miscellaneous AT string free of any predefined structure.
 *
 * Bytes written will be copied into the tx buffer "as-is" and may therefore
 * contain normally illegal character sequences.
 *
 * Consequently, the transceiver may not be able to determine the write-state
 * of the current write-sequence. It's therefore up to the API caller to provide
 * the correct write-state for subsequent write and flush calls. If the
 * write-sequence was initiated with the #ATTRANSCEIVER_WRITEOPTION_NOSTATE
 * option @p newWriteState will be ignored.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] data
 * Data buffer to be written. Will be interpreted as a byte buffer.
 * @param[in] length
 * Length of the @p data buffer in bytes.
 * @param[in] newWriteState
 * Write-state to be assumed by the transceiver after the write.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_PrepareWrite()
 */
Retcode_T AtTransceiver_Write(struct AtTransceiver_S *t, const void *data, size_t length, enum AtTransceiver_WriteState_E newWriteState);

/**
 * @brief Append an 8 bit signed integer value to the current AT command.
 *
 * If an AT parameter was inserted earlier and the write-sequence was not
 * initialized with the #ATTRANSCEIVER_WRITEOPTION_NOSTATE option set, this call
 * will automatically add the ',' separator to the sequence. Otherwise the
 * value is written directly to the current courser location.
 *
 * @par Single-argument example:
 * ```c
 * AtTransceiver_WriteSet(<set>);
 * AtTransceiver_WriteX(<parameter>);
 * // in tx buffer:
 * // AT<set>=<parameter>|
 * //                    ^ new write-courser
 * // Note: no ',' added!
 * ```
 *
 * @par Multi-argument example:
 * ```c
 * AtTransceiver_WriteSet(<set>);
 * AtTransceiver_WriteX(<parameter1>);
 * AtTransceiver_WriteX(<parameter2>);
 * // in tx buffer:
 * // AT<set>=<parameter1>,<parameter2>|
 * //                                  ^ new write-courser
 * // Note: ',' was added between <parameter1> and <parameter2>!
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #ATTRANSCEIVER_OCTAL
 * @see #ATTRANSCEIVER_DECIMAL
 * @see #ATTRANSCEIVER_HEXADECIMAL
 */
Retcode_T AtTransceiver_WriteI8(struct AtTransceiver_S *t, int8_t x, int base);

/**
 * @brief Append an 8 bit unsigned integer value to the current AT command.
 *
 * Behaviour is closely similar to #AtTransceiver_WriteI8(). Please refer to the
 * documentation there for more details.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #ATTRANSCEIVER_OCTAL
 * @see #ATTRANSCEIVER_DECIMAL
 * @see #ATTRANSCEIVER_HEXADECIMAL
 */
Retcode_T AtTransceiver_WriteU8(struct AtTransceiver_S *t, uint8_t x, int base);

/**
 * @brief Append a 16 bit signed integer value to the current AT command.
 *
 * Behaviour is closely similar to #AtTransceiver_WriteI8(). Please refer to the
 * documentation there for more details.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #ATTRANSCEIVER_OCTAL
 * @see #ATTRANSCEIVER_DECIMAL
 * @see #ATTRANSCEIVER_HEXADECIMAL
 */
Retcode_T AtTransceiver_WriteI16(struct AtTransceiver_S *t, int16_t x, int base);

/**
 * @brief Append a 16 bit unsigned integer value to the current AT command.
 *
 * Behaviour is closely similar to #AtTransceiver_WriteI8(). Please refer to the
 * documentation there for more details.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteI8()
 * @see #ATTRANSCEIVER_OCTAL
 * @see #ATTRANSCEIVER_DECIMAL
 * @see #ATTRANSCEIVER_HEXADECIMAL
 */
Retcode_T AtTransceiver_WriteU16(struct AtTransceiver_S *t, uint16_t x, int base);

/**
 * @brief Append a 32 bit signed integer value to the current AT command.
 *
 * Behaviour is closely similar to #AtTransceiver_WriteI8(). Please refer to the
 * documentation there for more details.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteI8()
 */
Retcode_T AtTransceiver_WriteI32(struct AtTransceiver_S *t, int32_t x, int base);

/**
 * @brief Append a 32 bit unsigned integer value to the current AT command.
 *
 * Behaviour is closely similar to AtTransceiver_WriteI8. Please refer to the
 * documentation there for more details.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] x
 * Value to be written.
 * @param[in] base
 * Numerical base (radix) in which the value should be formatted in. If this is
 * 0, decimal is implied. Supported values are 0, 8, 10, 16.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteI8()
 */
Retcode_T AtTransceiver_WriteU32(struct AtTransceiver_S *t, uint32_t x, int base);

/**
 * @brief Append a string value to the current AT command. The passed c-string
 * will be enclosed by quotes (`"`).
 *
 * If an AT parameter was inserted earlier and the write-sequence was not
 * initialized with the #ATTRANSCEIVER_WRITEOPTION_NOSTATE option set, this call
 * will automatically add the ',' separator to the sequence. Otherwise the
 * value is written directly to the current courser location.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] string
 * C-string to be written.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteHexString()
 */
Retcode_T AtTransceiver_WriteString(struct AtTransceiver_S *t, const char *string);

/**
 * @brief Append a hexadecimal-encoded byte string to the current AT command.
 * The resulting string will be enclosed by quotes (`"`).
 *
 * If an AT parameter was inserted earlier and the write-sequence was not
 * initialized with the #ATTRANSCEIVER_WRITEOPTION_NOSTATE option set, this call
 * will automatically add the ',' separator to the sequence. Otherwise the
 * value is written directly to the current courser location.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] data
 * Buffer to be encoded. Will be interpreted as a byte buffer.
 * @param[in] length
 * Length of buffer in bytes.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_WriteString()
 */
Retcode_T AtTransceiver_WriteHexString(struct AtTransceiver_S *t, const void *data, size_t length);

/**
 * @brief Finish the current AT command and pass contents of tx buffer to the
 * lower-level write function.
 *
 * Depending on the write options of the current write-sequence, this function
 * will send the tx buffer, wait, and immediately consume any echo coming from
 * the modem. Afterwards the caller may consume the potential modem response via
 * the read API.
 *
 * Flushing will implicitly close the current write-sequence and prepare a new
 * one based on the same options previously passed to
 * #AtTransceiver_PrepareWrite().
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @pre A preceding call to #AtTransceiver_PrepareWrite() must be made.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Flush(struct AtTransceiver_S *t, TickType_t timeout);

/**
 * @brief Skip the specified amount of bytes in the rx buffer.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] length
 * Number of bytes to skip.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_SkipArgument()
 * @see #AtTransceiver_SkipLine()
 */
Retcode_T AtTransceiver_SkipBytes(struct AtTransceiver_S *t, size_t length, TickType_t timeout);

/**
 * @brief Skip ahead until and including any ',' or `<S4>` character.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_SkipBytes()
 * @see #AtTransceiver_SkipLine()
 */
Retcode_T AtTransceiver_SkipArgument(struct AtTransceiver_S *t, TickType_t timeout);

/**
 * @brief Skip ahead until and including any `<S4>` character.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_SkipArgument()
 * @see #AtTransceiver_SkipBytes()
 */
Retcode_T AtTransceiver_SkipLine(struct AtTransceiver_S *t, TickType_t timeout);

/**
 * @brief Skip and read the next command string from the rx buffer and store it
 * in the given char buffer.
 *
 * A command is prefixed by '+' and terminated by ':'. The parsed command
 * string will be copied into the provided @p str `char` buffer. If more bytes
 * need to be copied than the `char` buffer can hold, a warning will be
 * returned.
 *
 * The output content of @p str is guaranteed to be a zero-terminated c-string
 * on non-error exit. This also applies, if the char buffer was to small to hold
 * the full command.
 *
 * In case the command could only be partially stored in the char buffer, the rx
 * buffer is still consumed until and including the ':' character. This is done,
 * so subsequent calls to read parameters may find the rx buffer in a valid
 * state.
 *
 * After successful return, the rx buffer may look like this:
 * ```plain
 * +<command>:|<arg1>,<arg2>,...,<argN>
 *            ^ new read-courser
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] str
 * Character buffer to hold the complete or partially parsed command string,
 * excluding '+' and ':` characters.
 * @param[in] length
 * Length of @p str buffer in bytes, accounting for zero-terminaton. The
 * effective character length of the resulting string may therefore only be
 * `length - 1`.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadCommand()
 */
Retcode_T AtTransceiver_ReadCommandAny(struct AtTransceiver_S *t, char *str, size_t length, TickType_t timeout);

/**
 * @brief Skip and read the next command string from the response.
 *
 * A command is prefixed by '+' and terminated by ':'. The parsed command is
 * expected to exactly match the passed c-string, otherwise an error is
 * returned.
 *
 * Even if the expected and parsed command do not match, the rx buffer will be
 * consumed until and including the next ':` character. This is done, so
 * subsequent calls to read arguments may find the rx buffer in a valid
 *
 * After successful return, the rx buffer may look like this:
 * ```plain
 * +<command>:|<arg1>,<arg2>,...,<argN>
 *            ^ We should be here.
 * ```
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[in] str
 * A c-string to exactly match against the parsed command string, excluding
 * '+' and ':` characters.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadCommandAny()
 */
Retcode_T AtTransceiver_ReadCommand(struct AtTransceiver_S *t, const char *str, TickType_t timeout);

/**
 * @brief Read the specified amount of bytes into a given buffer.
 *
 * This function does not perform any parsing, and simply passes through bytes
 * from the rx buffer.
 *
 * This may allow the caller to do parsing of non-standard AT responses.
 *
 * If the current content of rx buffer can not satisfy the requested amount, the
 * calling thread will be blocked until the specified @p timeout is exceeded. If
 * during this time further bytes become available, these will be stored in the
 * given @p data buffer. If the requested amount of bytes can not be provided
 * within the given @p timeout, the function will return with a timeout error.
 * Still, @p data may contain some amount of data, which can be
 * inspected and parsed using the @p numActualBytesRead out-parameter.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] data
 * Data buffer to hold the received data.
 * @param[in] length
 * Maximal number of bytes to read into the given data buffer.
 * @param[out] numActualBytesRead
 * If not set to `NULL`, will be set to the actual number of bytes read into the
 * data buffer before timeout is exceeded. If set to `NULL`, will be ignored.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Read(struct AtTransceiver_S *t, void *data, size_t length, size_t *numActualBytesRead, TickType_t timeout);

/**
 * @brief Read an AT response argument into a given char buffer.
 *
 * An AT argument is delimited by either `,` or `<S3><S4>` characters. Any
 * amount of leading or trailing whitespace sourrounding the argument is trimmed
 * off before being copied over into @p str buffer. On successful return @p str
 * is guaranteed to be zero-terminated.
 *
 * This function is specifically intended for certain non-standard AT string
 * arguments. Cases such as `+QCCID: 0123456789`, in which the AT string is
 * not sourounded by quotes for example.
 *
 * This next example shows how leading and trailing whitespace is handled:
 * ```plain
 * +XYZ:       Hello World     <S3><S4>
 * ```
 * To read this argument, a caller must provide a @p str buffer of at least
 * `11+1` bytes length (`+1` to account for zero-termination). Any leading
 * whitespace in front of `Hello` is trimmed, so is any trailing whitespace
 * following `World`. The whitspace in between the two words copied verbatim.
 * After successful completion the @p str buffer should contan `Hello World\0`.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] str
 * Character buffer to hold the trimmed argument.
 * @param[in] limit
 * Length of @p str buffer in bytes, accounting for zero-terminaton. The
 * effective character length of the resulting string may therefore only be
 * `limit - 1`. A value of zero is not allowed.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_ReadArgument(struct AtTransceiver_S *t, char *str, size_t limit, TickType_t timeout);

/**
 * @brief Read an AT response argument as string into a given char buffer.
 *
 * An AT string argument is enclosed by `"` characters. These characters will
 * be omitted when writing the string into the character buffer.
 *
 * The @p str buffer is guaranteed to be a zero-terminated c-string on
 * non-error exit. This also applies, if the character buffer was to small to
 * hold the full string.
 *
 * In case the string could only be partially stored in the char buffer, the rx
 * buffer is still consumed until and including the ',' or `<S4>` character.
 * This is done, so subsequent calls to read arguments may find the rx buffer in
 * a valid state.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] str
 * Character buffer to hold the complete or partially parsed string, excluding
 * `'"'` characters.
 * @param[in] limit
 * Length of @p str buffer in bytes, accounting for zero-terminaton. The
 * effective character length of the resulting string may therefore only be
 * `length - 1`. A value of zero is not allowed.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadHexString()
 */
Retcode_T AtTransceiver_ReadString(struct AtTransceiver_S *t, char *str, size_t limit, TickType_t timeout);

/**
 * @brief Read and decode an AT response argument as bytes into a given byte
 * buffer.
 *
 * An AT hex-string argument is enclosed by `"` characters. These characters
 * will be omitted when decoding and writing the string into the byte buffer.
 *
 * The decoded hex-data will not be zero-terminated. Instead use the
 * @p numActualBytesRead out-parameter to know how many bytes have been written
 * to the byte buffer.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] data
 * Data buffer to hold the complete or partially decoded data, excluding `'"'`.
 * Will be interpreted as byte buffer.
 * @param[in] limit
 * Length of @p data buffer in bytes.
 * @param[out] numActualBytesRead
 * Actual number of bytes written into data buffer, even if the function returns
 * an error. If `NULL`, will be ignored.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadString()
 */
Retcode_T AtTransceiver_ReadHexString(struct AtTransceiver_S *t, void *data, size_t limit, size_t *numActualBytesRead, TickType_t timeout);

/**
 * @brief Read an AT response argument as signed 8 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU32()
 */
Retcode_T AtTransceiver_ReadI8(struct AtTransceiver_S *t, int8_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response argument as unsigned 8 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU32()
 */
Retcode_T AtTransceiver_ReadU8(struct AtTransceiver_S *t, uint8_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response argument as signed 16 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU32()
 */
Retcode_T AtTransceiver_ReadI16(struct AtTransceiver_S *t, int16_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response argument as unsigned 16 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadI32()
 * @see #AtTransceiver_ReadU32()
 */
Retcode_T AtTransceiver_ReadU16(struct AtTransceiver_S *t, uint16_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response argument as signed 32 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadU32()
 */
Retcode_T AtTransceiver_ReadI32(struct AtTransceiver_S *t, int32_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response argument as unsigned 32 bit integer of specified
 * base.
 *
 * The function behaves very much like `strtol()` and follows the same syntax
 * requirements.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] x
 * On success, will be set to the value parsed from the AT response argument.
 * Otherwise the value is set to `0`.
 * @param[in] base
 * Numerical base (radix) that determines the valid characters and their
 * interpretation. If this is `0` the base used is determined by the format in
 * the sequence (see `strtol()`).
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 *
 * @see #AtTransceiver_ReadI8()
 * @see #AtTransceiver_ReadU8()
 * @see #AtTransceiver_ReadI16()
 * @see #AtTransceiver_ReadU16()
 * @see #AtTransceiver_ReadI32()
 */
Retcode_T AtTransceiver_ReadU32(struct AtTransceiver_S *t, uint32_t *x, int base, TickType_t timeout);

/**
 * @brief Read an AT response code in the form `<S3><S4><text><S3><S4>`.
 *
 * After successful completion, the full response code (including sourrounding
 * `<S3>` and `<S4>` characters) will be consumed from the rx buffer.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] code
 * On success, will be set to the parsed AT response code. If NULL, the parsed
 * code will be ignored, although it is still consumed from the rx buffer.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_ReadCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E *code, TickType_t timeout);

/**
 * @brief Checks if the rx-buffer's read-end currently points to end-of-line,
 * indicated by a pair of `<S3><S4>` characters.
 *
 * This function can not differentiate between the end of a response line
 * (`+CREG: 1, 2, 3`), or the end of a final or intermediate response code (such
 * as `OK`, `ERROR`, `CONNECT`, ...). It strictly checks for the presence of
 * both an `<S3>` @b and `<S4>` character at the current read-position.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 * @param[out] eol
 * Will be set to `true`, if currently pointing to `<S3><S4>` end-of-line
 * characters, otherwise `false`.
 * @param[in] timeout
 * Timeout in OS ticks.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_CheckEndOfLine(struct AtTransceiver_S *t, bool *eol, TickType_t timeout);

/**
 * @brief Unlock the transceiver instance and allow other threads to take over
 * ownership.
 *
 * @note If the current thread does not own the mutex, the behaviour is
 * undefined.
 *
 * @param[in,out] t
 * Instance of a transceiver on which to perfrom the action on.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Unlock(struct AtTransceiver_S *t);

/**
 * @brief Feed a byte buffer into the rx buffer of the specified transceiver
 * instance.
 *
 * If the rx buffer is full, or reaches its capacity limit during the feed, the
 * function returns an error. In any case though @p numActualBytesFed will
 * contain the number of bytes successfully written into the rx buffer. A caller
 * is encouraged to retry feeding at a later point in time, as the rx buffer is
 * gradually being consumed through the read API.
 *
 * This function is safe to call from interrupt context. Additionally the
 * transceiver instance does not have to be locked by the calling thread.
 *
 * @param[in,out] t
 * Instance of a transceiver into which to feed the given data buffer.
 * @param[in] data
 * Buffer containing feeding data. Will be interpreted as byte array.
 * @param[in] length
 * Length of data buffer.
 * @param[out] numActualBytesFed
 * Will be set to the number of bytes successfully written to the rx buffer,
 * even if the overall operation failed. If NULL, will be ignored.
 *
 * @return A #Retcode_T indicating the result of the action.
 */
Retcode_T AtTransceiver_Feed(struct AtTransceiver_S *t, const void *data, size_t length, size_t *numActualBytesFed);

/**
 * @brief Deinitialize a given transceiver instance, by freeing any dynamically
 * allocated buffers that were allocated through the transceivers
 * initialization.
 *
 * Any errors occurring during deinitialization will be passed onto the system
 * default error handler.
 *
 * @param[in,out] t
 * Transceiver instance to decomission.
 */
void AtTransceiver_Deinitialize(struct AtTransceiver_S *t);

/**
 * @brief Translate a given response code into its AT compliant numeric value.
 *
 * @param[in] code
 * Response code value to convert.
 *
 * @return Integer representation the given AT response code.
 */
int AtTransceiver_ResponseCodeAsNumeric(enum AtTransceiver_ResponseCode_E code);

/**
 * @brief Translate a given response code into its AT compliant textual value.
 *
 * @param[in] code
 * Response code value to convert.
 *
 * @return A c-string representation of the given AT response code.
 */
const char *AtTransceiver_ResponseCodeAsString(enum AtTransceiver_ResponseCode_E code);

/**
 * @brief Return whether or not the transceiver's read end currently points
 * just past a `<S4>` (newline) character.
 *
 * @pre The transceiver must be locked by the current thread.
 *
 * @param[in] t
 * Instance of a transceiver from which to return the state.
 *
 * @return `true` if the transceiver just crossed a `<S4>` (newline) character,
 * otherwise `false`.
 */
inline bool AtTransceiver_IsStartOfLine(const struct AtTransceiver_S *t)
{
    return t->StartOfLine;
}

#endif
/** @} */
