/*
 * Copyright (c) 2010-2019 Robert Bosch GmbH
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *     Robert Bosch GmbH - initial contribution
 */

#include "Kiso_CellularModules.h"
#define KISO_MODULE_ID KISO_CELLULAR_MODULE_ID_ATTRANSCEIVER

#include "AtTransceiver.h"
#include "AtUtils.h"

#include "Kiso_Basics.h"
#include "Kiso_Retcode.h"
#include "Kiso_Assert.h"

#include "Kiso_RingBuffer.h"
#include "Kiso_Logging.h"

#include "FreeRTOS.h"
#include "portmacro.h"
#include "semphr.h"
#include "task.h"

#include <stdio.h>

#define ATTRANSCEIVER_DUMMYBUFFERSIZE (1U)
static_assert(ATTRANSCEIVER_DUMMYBUFFERSIZE >= 1, "must be at least be one byte big");

#define ATTRANSCEIVER_STRINGIFY(x) #x
#define ATTRANSCEIVER_TOSTRING(x) (ATTRANSCEIVER_STRINGIFY(x))
#define ATTRANSCEIVER_CONCAT(x, y) x "" y
#define ATTRANSCEIVER_CHARIFY(s) (*(s))

#define ATTRANSCEIVER_ATTENTION "AT"
#define ATTRANSCEIVER_MNEMONICSTART "+"
#define ATTRANSCEIVER_SETSUFFIX "="
#define ATTRANSCEIVER_GETSUFFIX "?"
#define ATTRANSCEIVER_ARGSEPARATOR ","
#define ATTRANSCEIVER_STRLITERAL "\""
#define ATTRANSCEIVER_ARGLIST ":"
#define ATTRANSCEIVER_S3 "\r"
#define ATTRANSCEIVER_S4 "\n"
#define ATTRANSCEIVER_S3S4 ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_S3, ATTRANSCEIVER_S4)

#define ATTRANSCEIVER_MIN(A, B) ((A) > (B) ? (B) : (A))

struct AtTransceiver_ResponseCode_S
{
    enum AtTransceiver_ResponseCode_E Code;
    int Numeric;
    const char *Verbose;
};

static inline Retcode_T EnsureWriteState(struct AtTransceiver_S *t, enum AtTransceiver_WriteState_E writeState);
static inline Retcode_T WriteSeparatorIfNeeded(struct AtTransceiver_S *t);
static inline uint8_t NibbleFromHexChar(char hexChar);
static inline char HexCharFromNibble(uint8_t quintetIndex, uint8_t byte);
static inline Retcode_T HexToBin(const char *hex, void *bin, size_t binLength);
static inline Retcode_T BinToHex(const void *binData, size_t binLength, char *hexData, size_t hexLength);
static inline size_t FormatI32(int32_t x, int base, char *str, size_t len);
static inline size_t FormatU32(uint32_t x, int base, char *str, size_t len);
static size_t Read(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout);
static Retcode_T ReadUntil(struct AtTransceiver_S *t, void *buf, size_t *len, const void *until, TickType_t *timeout);
static Retcode_T SkipUntil(struct AtTransceiver_S *t, const void *anyOfThese, TickType_t *timeout);
static Retcode_T SkipAmount(struct AtTransceiver_S *t, size_t nbytes, TickType_t *timeout);

static const struct AtTransceiver_ResponseCode_S ResponseCodes[ATTRANSCEIVER_RESPONSECODE_MAX] = {
    {ATTRANSCEIVER_RESPONSECODE_OK, 0, "OK"},
    {ATTRANSCEIVER_RESPONSECODE_CONNECT, 1, "CONNECT"},
    {ATTRANSCEIVER_RESPONSECODE_RING, 2, "RING"},
    {ATTRANSCEIVER_RESPONSECODE_NOCARRIER, 3, "NO CARRIER"},
    {ATTRANSCEIVER_RESPONSECODE_ERROR, 4, "ERROR"},
    /* no number 5 */
    {ATTRANSCEIVER_RESPONSECODE_NODIALTONE, 6, "NO DIALTONE"},
    {ATTRANSCEIVER_RESPONSECODE_BUSY, 7, "BUSY"},
    {ATTRANSCEIVER_RESPONSECODE_NOANSWER, 8, "NO ANSWER"},
    {ATTRANSCEIVER_RESPONSECODE_CONNECTDR, 9, "CONNECT"},
    {ATTRANSCEIVER_RESPONSECODE_NOTSUPPORTED, 10, "NOT SUPPORT"},
    {ATTRANSCEIVER_RESPONSECODE_INVALIDCMDLINE, 11, "INVALID COMMAND LINE"},
    {ATTRANSCEIVER_RESPONSECODE_CR, 12, "CR"},
    {ATTRANSCEIVER_RESPONSECODE_SIMDROP, 13, "SIM DROP"},
    {ATTRANSCEIVER_RESPONSECODE_ABORTED, 3000, "Command aborted"}};

static inline Retcode_T EnsureWriteState(struct AtTransceiver_S *t, enum AtTransceiver_WriteState_E writeState)
{
    if (!(t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOSTATE))
    {
        return t->WriteState & writeState ? RETCODE_OK : RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
    return RETCODE_OK;
}

static inline Retcode_T WriteSeparatorIfNeeded(struct AtTransceiver_S *t)
{
    if (t->WriteState == ATTRANSCEIVER_WRITESTATE_ARGUMENT)
    {
        return AtTransceiver_Write(t, ATTRANSCEIVER_ARGSEPARATOR, sizeof(ATTRANSCEIVER_ARGSEPARATOR) - 1, t->WriteState);
    }
    return RETCODE_OK;
}

static inline uint8_t NibbleFromHexChar(char hexChar)
{
    uint8_t result;

    if ('0' <= hexChar && hexChar <= '9')
    {
        result = hexChar - '0';
    }
    else if ('A' <= hexChar && hexChar <= 'F')
    {
        result = 10 + (hexChar - 'A');
    }
    else if ('a' <= hexChar && hexChar <= 'f')
    {
        result = 10 + (hexChar - 'a');
    }
    else
    {
        result = UINT8_MAX;
    }

    return result;
}

static inline char HexCharFromNibble(uint8_t quintetIndex, uint8_t byte)
{
    const char *hexChars = "0123456789ABCDEF";
    uint8_t mask = (0 == quintetIndex) ? 0x0F : 0xF0;
    uint8_t shift = (0 == quintetIndex) ? 0 : 4;
    uint8_t quintet = (byte & mask) >> shift;

    return hexChars[quintet];
}

static inline Retcode_T HexToBin(const char *hex, void *bin, size_t binLength)
{
    for (uint32_t i = 0; i < binLength; i++)
    {
        uint8_t high = NibbleFromHexChar(hex[2 * i]);
        uint8_t low = NibbleFromHexChar(hex[2 * i + 1]);

        if (0xFF == high || 0xFF == low)
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
        }
        ((uint8_t *)bin)[i] = (high << 4) | low;
    }

    return RETCODE_OK;
}

static inline Retcode_T BinToHex(const void *binData, size_t binLength,
                                 char *hexData, size_t hexLength)
{
    if (hexLength < binLength * 2)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
    }
    for (uint32_t i = 0; i < binLength; i++)
    {
        hexData[2 * i] = HexCharFromNibble(1, ((const uint8_t *)binData)[i]);
        hexData[2 * i + 1] = HexCharFromNibble(0, ((const uint8_t *)binData)[i]);
    }
    return RETCODE_OK;
}

static inline size_t FormatI32(int32_t x, int base, char *str, size_t len)
{
    switch (base)
    {
    case 0:
    case ATTRANSCEIVER_DECIMAL:
        return snprintf(str, len, "%" PRIi32 "", x);
    case ATTRANSCEIVER_OCTAL:
        return snprintf(str, len, "%" PRIo32 "", x);
    case ATTRANSCEIVER_HEXADECIMAL:
        return snprintf(str, len, "%" PRIx32 "", x);
    default:
        return 0;
    }
}

static inline size_t FormatU32(uint32_t x, int base, char *str, size_t len)
{
    switch (base)
    {
    case 0:
    case ATTRANSCEIVER_DECIMAL:
        return snprintf(str, len, "%" PRIu32 "", x);
    case ATTRANSCEIVER_OCTAL:
        return snprintf(str, len, "%" PRIo32 "", x);
    case ATTRANSCEIVER_HEXADECIMAL:
        return snprintf(str, len, "%" PRIx32 "", x);
    default:
        return 0;
    }
}

static size_t Read(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout)
{
    TickType_t totalTicksSlept = 0;
    const TickType_t initialTimeout = timeout != NULL ? *timeout : 0;
    size_t total = 0;
    bool hitTimeout = false;
    while (total < len && !hitTimeout)
    {
        TickType_t effectiveTimeout = timeout != NULL ? *timeout : 0;
        size_t bytesRead = (size_t)RingBuffer_Read(&t->RxRingBuffer, (uint8_t *)buf + total, len - total);
        total += bytesRead;
        if (initialTimeout <= 0 || effectiveTimeout <= 0)
        {
            /* Requested with no-wait, so we just return with what we have. */
            hitTimeout = true;
        }
        else if (total < len)
        {
            TickType_t preWait = xTaskGetTickCount();
            BaseType_t wokenInTime = xSemaphoreTake(t->RxWakeupHandle, effectiveTimeout);
            if (pdTRUE == wokenInTime)
            {
                TickType_t ticksSlept = xTaskGetTickCount() - preWait;
                totalTicksSlept += ticksSlept;
                if (totalTicksSlept > initialTimeout)
                {
                    hitTimeout = true;
                }
                assert(*timeout >= ticksSlept);
                *timeout -= ticksSlept;
            }
            else
            {
                hitTimeout = true;
            }
        }
    }

    return total;
}

static Retcode_T ReadUntil(struct AtTransceiver_S *t, void *buf, size_t *len, const void *anyOfThese, TickType_t *timeout)
{
    bool matchFound = false;
    size_t i;
    size_t r = 0;
    for (i = 0; i < *len && !matchFound; i += r)
    {
        /** \todo: Consider implementing peek feature instead of using Read.
         * This could improve performance when combined with larger dummy
         * buffers.
         * The current implementation tries to anticipate a "c" buffer with size
         * larger than one byte (that's why we use memchr). The problem is
         * though, without a proper peek function, we may consume too many
         * bytes from the RingBuffer (beyond the first occurrence of
         * anyOfThese).
         *
         * This may illustrate the issue further:
         *   ReadUntil(':') with dummy buffer of 4 byte length
         *   > + C O P S : 1 , 2 , 3 , 4
         *    ^ - We start of here, at the beginning.
         *   > _ _ _ _ S : 1 , 2 , 3 , 4
         *            ^ - We have consumed the first 4 bytes.
         *   > _ _ _ _ _ _ _ _ 2 , 3 , 4
         *               X    ^ - Oops we've gone too far! Yes, we did encounter
         *                        the ':' at X but kept on reading into our
         *                        dummy buffer.
         *
         * We can't simply put these bytes back in the RingBuffer
         * So we effectively "steal" bytes from subsequent Read calls.
         */
        char c;
        r = Read(t, &c, ATTRANSCEIVER_MIN(sizeof(c), *len - i), timeout);
        if (!r)
        {
            *len = i;
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
        }

        for (size_t j = 0; ((const char *)anyOfThese)[j] != '\0' && !matchFound; ++j)
        {
            void *match = memchr(&c, ((const char *)anyOfThese)[j], r);
            if (match)
            {
                matchFound = true;
                r = ((char *)match) - &c;
            }
        }
        memcpy(((uint8_t *)buf) + i, &c, r);
    }
    if (matchFound)
    {
        *len = i;
        return RETCODE_OK;
    }
    else
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
    }
}

static Retcode_T SkipUntil(struct AtTransceiver_S *t, const void *anyOfThese, TickType_t *timeout)
{
    uint8_t dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
    Retcode_T rc;
    do
    {
        size_t len = ATTRANSCEIVER_DUMMYBUFFERSIZE;
        rc = ReadUntil(t, dummy, &len, anyOfThese, timeout);
    } while (RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES) == rc);
    return rc;
}

static Retcode_T SkipAmount(struct AtTransceiver_S *t, size_t nbytes, TickType_t *timeout)
{
    uint8_t dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
    size_t r = 0;
    for (; nbytes > 0; nbytes -= r)
    {
        r = Read(t, dummy, ATTRANSCEIVER_DUMMYBUFFERSIZE > nbytes ? nbytes : ATTRANSCEIVER_DUMMYBUFFERSIZE, timeout);
        if (!r)
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
        }
    }

    return RETCODE_OK;
}

Retcode_T AtTransceiver_Initialize(struct AtTransceiver_S *t,
                                   void *rxBuffer, size_t rxLength,
                                   AtTransceiver_WriteFunction_T writeFunc)
{
    t->RxBufferLength = rxLength;
    RingBuffer_Initialize(&t->RxRingBuffer, (uint8_t *)rxBuffer, rxLength);

    t->Write = writeFunc;

    t->RxWakeupHandle = xSemaphoreCreateBinaryStatic(&t->RxWakeupBuffer);
    assert(NULL != t->RxWakeupHandle); /* due to static allocation */

    t->TxWakeupHandle = xSemaphoreCreateBinaryStatic(&t->TxWakeupBuffer);
    assert(NULL != t->TxWakeupHandle); /* due to static allocation */

    t->LockHandle = xSemaphoreCreateMutexStatic(&t->LockBuffer);
    assert(NULL != t->LockHandle); /* due to static allocation */

    t->WriteState = ATTRANSCEIVER_WRITESTATE_START;

    return RETCODE_OK;
}

Retcode_T AtTransceiver_Lock(struct AtTransceiver_S *t)
{
    (void)xSemaphoreTake(t->LockHandle, portMAX_DELAY);
    return RETCODE_OK;
}

Retcode_T AtTransceiver_TryLock(struct AtTransceiver_S *t, TickType_t timeout)
{
    BaseType_t taken = xSemaphoreTake(t->LockHandle, timeout);
    return taken == pdTRUE ? RETCODE_OK : RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
}

Retcode_T AtTransceiver_PrepareWrite(struct AtTransceiver_S *t, enum AtTransceiver_WriteOption_E options, void *txBuffer, size_t txLength)
{
    if (!(options & ATTRANSCEIVER_WRITEOPTION_NOBUFFER))
    {
        t->TxBuffer = txBuffer;
        t->TxBufferLength = txLength;
    }
    t->TxBufferUsed = 0; /* Always reset TxBufferUsed, even if we don't have a TxBuffer. */
    t->WriteOptions = options;
    t->WriteState = options & ATTRANSCEIVER_WRITEOPTION_NOSTATE ? ATTRANSCEIVER_WRITESTATE_INVALID : ATTRANSCEIVER_WRITESTATE_START;
    return RETCODE_OK;
}

Retcode_T AtTransceiver_WriteAction(struct AtTransceiver_S *t, const char *action)
{
    Retcode_T rc = EnsureWriteState(t, ATTRANSCEIVER_WRITESTATE_START);
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_ATTENTION, sizeof(ATTRANSCEIVER_ATTENTION) - 1, t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, action, strlen(action), ATTRANSCEIVER_WRITESTATE_END);
    }
    return rc;
}

Retcode_T AtTransceiver_WriteSet(struct AtTransceiver_S *t, const char *set)
{
    Retcode_T rc = EnsureWriteState(t, ATTRANSCEIVER_WRITESTATE_START);
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_ATTENTION, sizeof(ATTRANSCEIVER_ATTENTION) - 1, t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, set, strlen(set), t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_SETSUFFIX, sizeof(ATTRANSCEIVER_SETSUFFIX) - 1, ATTRANSCEIVER_WRITESTATE_COMMAND);
    }
    return rc;
}

Retcode_T AtTransceiver_WriteGet(struct AtTransceiver_S *t, const char *get)
{
    Retcode_T rc = EnsureWriteState(t, ATTRANSCEIVER_WRITESTATE_START);
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_ATTENTION, sizeof(ATTRANSCEIVER_ATTENTION) - 1, t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, get, strlen(get), t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_GETSUFFIX, sizeof(ATTRANSCEIVER_GETSUFFIX) - 1, ATTRANSCEIVER_WRITESTATE_END);
    }
    return rc;
}

Retcode_T AtTransceiver_Write(struct AtTransceiver_S *t, const void *data, size_t length, enum AtTransceiver_WriteState_E newWriteState)
{
    Retcode_T rc = RETCODE_OK;
    size_t bytesStoredOrSent = 0;
    if (t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOBUFFER)
    {
        rc = t->Write(data, length, NULL);
        bytesStoredOrSent = length;
    }
    else
    {
        assert(t->TxBufferUsed <= t->TxBufferLength);
        size_t minLen = ATTRANSCEIVER_MIN(length, t->TxBufferLength - t->TxBufferUsed);
        if (minLen != length)
        {
            /* Set error, but continue copying however much still fits in the
             * buffer. */
            rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
        }
        memcpy(t->TxBuffer + t->TxBufferUsed, data, minLen);
        bytesStoredOrSent = minLen;
    }
    /* Keep track of the number of bytes put in TxBuffer/sent out over Write.
     * Otherwise we don't know how many bytes to consume on echo response. If
     * ATTRANSCEIVER_WRITEOPTION_NOBUFFER is set, we will use TxBufferUsed to
     * skip the echo response. */
    t->TxBufferUsed += bytesStoredOrSent;
    if (!(t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOSTATE))
    {
        t->WriteState = newWriteState;
    }
    return rc;
}

Retcode_T AtTransceiver_WriteI8(struct AtTransceiver_S *t, int8_t x, int base)
{
    int32_t _x = x;
    return AtTransceiver_WriteI32(t, _x, base);
}

Retcode_T AtTransceiver_WriteI16(struct AtTransceiver_S *t, int16_t x, int base)
{
    int32_t _x = x;
    return AtTransceiver_WriteI32(t, _x, base);
}

Retcode_T AtTransceiver_WriteI32(struct AtTransceiver_S *t, int32_t x, int base)
{
    size_t len = FormatI32(x, base, NULL, 0);
    Retcode_T rc = RETCODE_OK;
    if (len <= 0)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    char stringify[len + 1];
    if (RETCODE_OK == rc)
    {
        rc = EnsureWriteState(t, (enum AtTransceiver_WriteState_E)(ATTRANSCEIVER_WRITESTATE_COMMAND | ATTRANSCEIVER_WRITESTATE_ARGUMENT));
    }
    if (RETCODE_OK == rc)
    {
        rc = WriteSeparatorIfNeeded(t);
    }
    if (RETCODE_OK == rc)
    {
        len = FormatI32(x, base, stringify, sizeof(stringify));
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, stringify, len, ATTRANSCEIVER_WRITESTATE_ARGUMENT);
    }
    return rc;
}

Retcode_T AtTransceiver_WriteU8(struct AtTransceiver_S *t, uint8_t x, int base)
{
    uint32_t _x = x;
    return AtTransceiver_WriteU32(t, _x, base);
}

Retcode_T AtTransceiver_WriteU16(struct AtTransceiver_S *t, uint16_t x, int base)
{
    uint32_t _x = x;
    return AtTransceiver_WriteU32(t, _x, base);
}

Retcode_T AtTransceiver_WriteU32(struct AtTransceiver_S *t, uint32_t x, int base)
{
    size_t len = FormatU32(x, base, NULL, 0);
    Retcode_T rc = RETCODE_OK;
    if (len <= 0)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }
    char stringify[len + 1];
    if (RETCODE_OK == rc)
    {
        rc = EnsureWriteState(t, (enum AtTransceiver_WriteState_E)(ATTRANSCEIVER_WRITESTATE_COMMAND | ATTRANSCEIVER_WRITESTATE_ARGUMENT));
    }
    if (RETCODE_OK == rc)
    {
        rc = WriteSeparatorIfNeeded(t);
    }
    if (RETCODE_OK == rc)
    {
        len = FormatU32(x, base, stringify, sizeof(stringify));
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, stringify, len, ATTRANSCEIVER_WRITESTATE_ARGUMENT);
    }
    return rc;
}

Retcode_T AtTransceiver_WriteString(struct AtTransceiver_S *t, const char *string)
{
    Retcode_T rc = EnsureWriteState(t, (enum AtTransceiver_WriteState_E)(ATTRANSCEIVER_WRITESTATE_COMMAND | ATTRANSCEIVER_WRITESTATE_ARGUMENT));
    if (RETCODE_OK == rc)
    {
        rc = WriteSeparatorIfNeeded(t);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_STRLITERAL, sizeof(ATTRANSCEIVER_STRLITERAL) - 1, t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, string, strlen(string), t->WriteState);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_STRLITERAL, sizeof(ATTRANSCEIVER_STRLITERAL) - 1, ATTRANSCEIVER_WRITESTATE_ARGUMENT);
    }

    return rc;
}

Retcode_T AtTransceiver_WriteHexString(struct AtTransceiver_S *t, const void *data, size_t length)
{
    Retcode_T rc = EnsureWriteState(t, (enum AtTransceiver_WriteState_E)(ATTRANSCEIVER_WRITESTATE_COMMAND | ATTRANSCEIVER_WRITESTATE_ARGUMENT));
    if (RETCODE_OK == rc)
    {
        rc = WriteSeparatorIfNeeded(t);
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_STRLITERAL, sizeof(ATTRANSCEIVER_STRLITERAL) - 1, t->WriteState);
    }
    char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE * 2];
    size_t written = 0;
    while (RETCODE_OK == rc && written < length)
    {
        size_t min = ATTRANSCEIVER_MIN(length - written, sizeof(dummy) / 2);
        rc = BinToHex(data + written, min, dummy, sizeof(dummy));
        if (RETCODE_OK == rc)
        {
            rc = AtTransceiver_Write(t, dummy, min * 2, t->WriteState);
        }
        if (RETCODE_OK == rc)
        {
            written += min;
        }
    }
    if (RETCODE_OK == rc)
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_STRLITERAL, sizeof(ATTRANSCEIVER_STRLITERAL) - 1, ATTRANSCEIVER_WRITESTATE_ARGUMENT);
    }

    return rc;
}

Retcode_T AtTransceiver_Flush(struct AtTransceiver_S *t, TickType_t timeout)
{
    Retcode_T rc = RETCODE_OK;
    if (!(t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOFINALS3S4))
    {
        rc = AtTransceiver_Write(t, ATTRANSCEIVER_S3S4, sizeof(ATTRANSCEIVER_S3S4) - 1, ATTRANSCEIVER_WRITESTATE_END);
    }
    if (RETCODE_OK == rc && !(t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOBUFFER))
    {
        rc = t->Write(t->TxBuffer, t->TxBufferUsed, NULL);
    }
    if (RETCODE_OK == rc && !(t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOECHO))
    {
        if (t->WriteOptions & ATTRANSCEIVER_WRITEOPTION_NOBUFFER)
        {
            /* We can't verify the echo, as we have no record of what we sent
             * over the course of this write sequence. Our only option is to
             * skip the expected number of bytes from the echo response, so the
             * read sequence can proceed. */
            rc = SkipAmount(t, t->TxBufferUsed, &timeout);
        }
        else
        {
            char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
            size_t r = 0;
            size_t totalRead = 0;
            do
            {
                r = Read(t, dummy, ATTRANSCEIVER_MIN(t->TxBufferUsed - totalRead, sizeof(dummy)), &timeout);
                if (!r)
                {
                    rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
                    break;
                }
                else if (memcmp(dummy, t->TxBuffer + totalRead, r))
                {
                    rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
                    break;
                }
                totalRead += r;
            } while (r > 0 && totalRead < t->TxBufferUsed);
        }
    }
    if (RETCODE_OK == rc)
    {
        /* Flush complete, make sure to prepare the next write-sequence. */
        t->TxBufferUsed = 0;
    }
    return rc;
}

Retcode_T AtTransceiver_SkipBytes(struct AtTransceiver_S *t, size_t length, TickType_t timeout)
{
    return SkipAmount(t, length, &timeout);
}

Retcode_T AtTransceiver_SkipArgument(struct AtTransceiver_S *t, TickType_t timeout)
{
    return SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout);
}

Retcode_T AtTransceiver_ReadCommandAny(struct AtTransceiver_S *t, char *str, size_t length, TickType_t timeout)
{
    /* A singular response typically looks like this:
     * <S3><S4>+<command>:<attribute_list><S3><S4>
     */
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_MNEMONICSTART, &timeout);
    if (RETCODE_OK != rc)
        return rc;
    /* <S3><S4>+|<command>:<attribute_list><S3><S4>
     *          ^ We should now be here.
     */

    size_t r = length;
    rc = ReadUntil(t, str, &r, ATTRANSCEIVER_ARGLIST, &timeout);
    if (RETCODE_OK == rc)
    {
        /* <S3><S4>+<command>:|<attribute_list><S3><S4>
         *                    ^ We should now be here.
         */
        if (r >= length)
        {
            rc = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES);
        }
        if (length > 0)
            str[r] = '\0';
    }
    else if (RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES) == rc)
    {
        /* <S3><S4>+<command>:<attribute_list><S3><S4>
         *              ^ We are somewhere in here, our buffer was too small.
         * Looks like we'll have to do some clean-up to not screw up any
         * subsequent API calls.
         */
        if (length > 0)
            str[r - 1] = '\0';
        rc = SkipUntil(t, ATTRANSCEIVER_ARGLIST, &timeout);
        if (RETCODE_OK == rc)
        {
            /* <S3><S4>+<command>:|<attribute_list><S3><S4>
             *                    ^ We should now be here
             */
            rc = RETCODE(RETCODE_SEVERITY_WARNING, RETCODE_OUT_OF_RESOURCES);
        }
    }

    return rc;
}

Retcode_T AtTransceiver_ReadCommand(struct AtTransceiver_S *t, const char *str, TickType_t timeout)
{
    /* A singular response typically looks like this:
     * <S3><S4>+<command>:<attribute_list><S3><S4>
     */
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_MNEMONICSTART, &timeout);
    if (RETCODE_OK != rc)
        return rc;
    /* <S3><S4>+|<command>:<attribute_list><S3><S4>
     *          ^ We should now be here.
     */

    size_t len = strlen(str);
    size_t i;
    size_t r;
    for (i = 0; i < len; i += r)
    {
        char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
        r = Read(t, dummy, ATTRANSCEIVER_MIN(sizeof(dummy), len - i), &timeout);
        if (r)
        {
            for (size_t j = 0; j < r; ++j)
            {
                if (dummy[j] != str[i + j] || ATTRANSCEIVER_CHARIFY(ATTRANSCEIVER_ARGLIST) == dummy[j])
                    return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
            }
        }
        else
        {
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
        }
    }
    /* <S3><S4>+<command>|:<attribute_list><S3><S4>
     *                   ^ We should now be here.
     * Meaning we still have to clean up the ':' before we leave.
     */

    if (str[i] != '\0')
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    else
        return SkipUntil(t, ATTRANSCEIVER_ARGLIST, &timeout); /* skip until beginning of attribute */
}

Retcode_T AtTransceiver_Read(struct AtTransceiver_S *t, void *data, size_t length, size_t *numActualBytesRead, TickType_t timeout)
{
    size_t r = Read(t, data, length, &timeout);
    if (numActualBytesRead != NULL)
        *numActualBytesRead = r;
    if (r != length)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    else
        return RETCODE_OK;
}

Retcode_T AtTransceiver_ReadString(struct AtTransceiver_S *t, char *str, size_t limit, TickType_t timeout)
{
    char c = 0;
    size_t r = Read(t, &c, sizeof(c), &timeout);
    if (!r)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    if ('"' != c)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);

    Retcode_T rc = ReadUntil(t, str, &limit, "\"", &timeout);
    if (RETCODE_OK == rc)
    {
        str[limit] = '\0';
        rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout); /* skip until end of attribute */
    }
    return rc;
}

Retcode_T AtTransceiver_ReadHexString(struct AtTransceiver_S *t, void *data, size_t limit, size_t *numActualBytesRead, TickType_t timeout)
{
    char c = 0;
    size_t r = Read(t, &c, sizeof(c), &timeout);
    if (!r)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    if ('"' != c)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);

    char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE * 2];
    Retcode_T rc = RETCODE_OK;
    for (size_t i = 0; i < limit && RETCODE_OK == rc; i += r / 2, numActualBytesRead != NULL ? (*numActualBytesRead) += r / 2 : 0)
    {
        r = sizeof(dummy);
        rc = ReadUntil(t, ((uint8_t *)dummy), &r, "\"", &timeout);
        if (RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES) == rc)
        {
            rc = RETCODE_OK; /* don't worry, we expected data to be too small to hold the full hex string. */
        }

        if (RETCODE_OK == rc)
        {
            rc = HexToBin(dummy, ((uint8_t *)data) + i, r / 2);
        }
    }

    if (RETCODE_OK == rc)
    {
        rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout); /* skip until end of attribute */
    }

    return rc;
}

Retcode_T AtTransceiver_ReadI8(struct AtTransceiver_S *t, int8_t *x, int base, TickType_t timeout)
{
    int32_t _x = 0;
    Retcode_T rc = AtTransceiver_ReadI32(t, &_x, base, timeout);
    *x = _x;
    return rc;
}
Retcode_T AtTransceiver_ReadU8(struct AtTransceiver_S *t, uint8_t *x, int base, TickType_t timeout)
{
    uint32_t _x = 0;
    Retcode_T rc = AtTransceiver_ReadU32(t, &_x, base, timeout);
    *x = _x;
    return rc;
}

Retcode_T AtTransceiver_ReadI16(struct AtTransceiver_S *t, int16_t *x, int base, TickType_t timeout)
{
    int32_t _x = 0;
    Retcode_T rc = AtTransceiver_ReadI32(t, &_x, base, timeout);
    *x = _x;
    return rc;
}
Retcode_T AtTransceiver_ReadU16(struct AtTransceiver_S *t, uint16_t *x, int base, TickType_t timeout)
{
    uint32_t _x = 0;
    Retcode_T rc = AtTransceiver_ReadU32(t, &_x, base, timeout);
    *x = _x;
    return rc;
}

Retcode_T AtTransceiver_ReadI32(struct AtTransceiver_S *t, int32_t *x, int base, TickType_t timeout)
{
    char buf[sizeof(ATTRANSCEIVER_TOSTRING(INT32_MAX)) + 1];
    size_t len = sizeof(buf) - 1;
    Retcode_T rc = ReadUntil(t, buf, &len, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout);
    if (RETCODE_OK != rc)
    {
        return rc;
    }
    buf[len] = '\0';

    char *end = NULL;
    int _x = strtol(buf, &end, base);
    if (buf == end)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
    else
    {
        *x = _x;
    }

    return RETCODE_OK;
}
Retcode_T AtTransceiver_ReadU32(struct AtTransceiver_S *t, uint32_t *x, int base, TickType_t timeout)
{
    char buf[sizeof(ATTRANSCEIVER_TOSTRING(INT32_MAX)) + 1];
    size_t len = sizeof(buf) - 1;
    Retcode_T rc = ReadUntil(t, buf, &len, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout);
    if (RETCODE_OK != rc)
    {
        return rc;
    }
    buf[len] = '\0';

    char *end = NULL;
    int _x = strtoul(buf, &end, base);
    if (buf == end)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
    else
    {
        *x = _x;
    }

    return RETCODE_OK;
}

Retcode_T AtTransceiver_ReadCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E *code, TickType_t timeout)
{
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &timeout);
    if (rc)
        return rc;
    /* Skipped preceeding '<S3><S4>' in theoretical response:
     * <S3><S4>|<verbose_code><S3><S4>
     *         ^ We should now be here. */
    char dummy[sizeof("INVALID COMMAND LINE")]; /* size of biggest verbose response code + '\0' */
    size_t len = sizeof(dummy);
    rc = ReadUntil(t, dummy, &len, "\r", &timeout);
    if (rc)
        return rc;

    enum AtTransceiver_ResponseCode_E c = ATTRANSCEIVER_RESPONSECODE_MAX;

    /* we only handle verbose response codes for now */
    for (size_t i = 0; i < ATTRANSCEIVER_RESPONSECODE_MAX && c == ATTRANSCEIVER_RESPONSECODE_MAX; ++i)
    {
        if (0 == strncmp(ResponseCodes[i].Verbose, dummy, strlen(ResponseCodes[i].Verbose)))
        {
            /* in case of CONNECT we have to decide between CONNECT and CONNECTDR */
            if (ResponseCodes[i].Code == ATTRANSCEIVER_RESPONSECODE_CONNECT)
            {
                if (len == strlen(ResponseCodes[i].Verbose))
                {
                    c = ATTRANSCEIVER_RESPONSECODE_CONNECT;
                }
                else
                {
                    /* ignore the data-rate argument */
                    c = ATTRANSCEIVER_RESPONSECODE_CONNECTDR;
                }
            }
            else
            {
                c = ResponseCodes[i].Code;
            }
        }
    }

    if (c != ATTRANSCEIVER_RESPONSECODE_MAX)
    {
        if (code)
            *code = c;
        return SkipUntil(t, ATTRANSCEIVER_S4, &timeout);
    }
    else
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
}

Retcode_T AtTransceiver_Unlock(struct AtTransceiver_S *t)
{
    (void)xSemaphoreGive(t->LockHandle);
    return RETCODE_OK;
}

Retcode_T AtTransceiver_Feed(struct AtTransceiver_S *t, const void *data, size_t length, size_t *numActualBytesFed)
{
    size_t _numActualBytesFed = RingBuffer_Write(&t->RxRingBuffer, (const uint8_t *)data, length);
    if (numActualBytesFed)
        *numActualBytesFed = _numActualBytesFed;
    return RETCODE_OK;
}

void AtTransceiver_Deinitialize(struct AtTransceiver_S *t)
{
    KISO_UNUSED(t);
}

int AtTransceiver_ResponseCodeAsNumeric(enum AtTransceiver_ResponseCode_E code)
{
    return ResponseCodes[code].Numeric;
}

const char *AtTransceiver_ResponseCodeAsString(enum AtTransceiver_ResponseCode_E code)
{
    return ResponseCodes[code].Verbose;
}
