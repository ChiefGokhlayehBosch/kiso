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
#define ATTRANSCEIVER_SKIPEMPTYLINESLIMIT (5U)

#define ATTRANSCEIVER_STRINGIFY(x) #x
#define ATTRANSCEIVER_TOSTRING(x) (ATTRANSCEIVER_STRINGIFY(x))
#define ATTRANSCEIVER_CONCAT(x, y) x "" y
#define ATTRANSCEIVER_CONCAT3(x, y, z) ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_CONCAT(x, y), z)
#define ATTRANSCEIVER_CONCAT4(w, x, y, z) ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_CONCAT3(w, x, y), z)
#define ATTRANSCEIVER_CHARIFY(s) (*(s))

#define ATTRANSCEIVER_ATTENTION "AT"
#define ATTRANSCEIVER_MNEMONICSTART "+"
#define ATTRANSCEIVER_SETSUFFIX "="
#define ATTRANSCEIVER_GETSUFFIX "?"
#define ATTRANSCEIVER_ARGSEPARATOR ","
#define ATTRANSCEIVER_WORDSEPARATOR " "
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

static inline void UpdateStartOfLineState(struct AtTransceiver_S *t, const void *needle);
static inline Retcode_T EnsureWriteState(struct AtTransceiver_S *t, enum AtTransceiver_WriteState_E writeState);
static inline Retcode_T WriteSeparatorIfNeeded(struct AtTransceiver_S *t);
static inline uint8_t NibbleFromHexChar(char hexChar);
static inline char HexCharFromNibble(uint8_t quintetIndex, uint8_t byte);
static inline Retcode_T HexToBin(const char *hex, void *bin, size_t binLength);
static inline Retcode_T BinToHex(const void *binData, size_t binLength, char *hexData, size_t hexLength);
static inline size_t FormatI32(int32_t x, int base, char *str, size_t len);
static inline size_t FormatU32(uint32_t x, int base, char *str, size_t len);
static size_t Peek(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout);
static size_t Pop(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout);
static size_t Skip(struct AtTransceiver_S *t, size_t len, TickType_t *timeout);
static Retcode_T PopUntil(struct AtTransceiver_S *t, void *buf, size_t *len, const void *anyOfThese, const void **foundNeedle, TickType_t *timeout);
static Retcode_T SkipUntil(struct AtTransceiver_S *t, const void *anyOfThese, const void **foundNeedle, TickType_t *timeout);

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
    /* no number 14-16 */
    {ATTRANSCEIVER_RESPONSECODE_SENDOK, 17, "SEND OK"},     /* The datasheet doesn't state what the numeric value of "SEND OK" is. The value here has been figured out through testing. */
    {ATTRANSCEIVER_RESPONSECODE_SENDFAIL, 18, "SEND FAIL"}, /* The datasheet doesn't state what the numeric value of "SEND FAIL" is. TODO: Figure out numeric value of "SEND FAIL" */
    {ATTRANSCEIVER_RESPONSECODE_ABORTED, 3000, "Command aborted"}};

static inline void UpdateStartOfLineState(struct AtTransceiver_S *t, const void *needle)
{
    t->StartOfLine = ATTRANSCEIVER_S4[0] == ((const char *)needle)[0];
}

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

static bool WaitForMoreRx(SemaphoreHandle_t wakeupSignal, TickType_t maxTimeout, TickType_t *totalTicksSlept)
{
    assert(NULL != totalTicksSlept);

    bool hitTimeout = false;
    TickType_t remainingTimeout = maxTimeout > *totalTicksSlept ? maxTimeout - *totalTicksSlept : 0;

    if (remainingTimeout <= 0)
    {
        /* Requested with no-wait, so we just return with what we have. */
        hitTimeout = true;
    }
    else
    {
        TickType_t preWait = xTaskGetTickCount();
        BaseType_t wokenInTime = xSemaphoreTake(wakeupSignal, remainingTimeout);
        if (pdTRUE == wokenInTime)
        {
            TickType_t ticksSlept = xTaskGetTickCount() - preWait;
            *totalTicksSlept += ticksSlept;
            if (*totalTicksSlept > maxTimeout)
            {
                hitTimeout = true;
            }
        }
        else
        {
            /* Assumption: xSemaphoreTake should only return false if timeout
             * was exceeded. */
            hitTimeout = true;
        }
    }

    return hitTimeout;
}

static size_t Peek(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout)
{
    TickType_t totalTicksSlept = 0;
    const TickType_t maxTimeout = timeout != NULL ? *timeout : 0;
    size_t total = 0;
    bool hitTimeout = false;
    while (total < len && !hitTimeout)
    {
        total = (size_t)RingBuffer_Peek(&t->RxRingBuffer, (uint8_t *)buf, len);

        if (total < len)
        {
            hitTimeout = WaitForMoreRx(t->RxWakeupHandle, maxTimeout, &totalTicksSlept);
        }
    }

    return total;
}

static size_t Pop(struct AtTransceiver_S *t, void *buf, size_t len, TickType_t *timeout)
{
    TickType_t totalTicksSlept = 0;
    const TickType_t maxTimeout = timeout != NULL ? *timeout : 0;
    size_t total = 0;
    bool hitTimeout = false;
    while (total < len && !hitTimeout)
    {
        total += (size_t)RingBuffer_Read(&t->RxRingBuffer, (uint8_t *)buf + total, len - total);

        if (total < len)
        {
            hitTimeout = WaitForMoreRx(t->RxWakeupHandle, maxTimeout, &totalTicksSlept);
        }
    }

    return total;
}

static size_t Skip(struct AtTransceiver_S *t, size_t len, TickType_t *timeout)
{
    TickType_t totalTicksSlept = 0;
    const TickType_t maxTimeout = timeout != NULL ? *timeout : 0;
    size_t total = 0;
    bool hitTimeout = false;
    char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
    while (total < len && !hitTimeout)
    {
        size_t bytesRead = (size_t)RingBuffer_Read(&t->RxRingBuffer, (uint8_t *)dummy, ATTRANSCEIVER_MIN(sizeof(dummy), len - total));
        total += bytesRead;

        if (!bytesRead && total < len)
        {
            hitTimeout = WaitForMoreRx(t->RxWakeupHandle, maxTimeout, &totalTicksSlept);
        }
    }

    return total;
}

static Retcode_T PopUntil(struct AtTransceiver_S *t, void *buf, size_t *len, const void *anyOfThese, const void **foundNeedle, TickType_t *timeout)
{
    size_t i;
    size_t r = 0;
    void *lastMatch = NULL;
    for (i = 0; i < *len && !lastMatch; i += r)
    {
        r = Peek(t, buf + i, *len - i, timeout);
        if (!r)
        {
            *len = i;
            return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
        }

        for (size_t j = 0; ((const char *)anyOfThese)[j] != '\0'; ++j)
        {
            void *match = memchr(buf + i, ((const char *)anyOfThese)[j], r);
            if (match && (!lastMatch || match < lastMatch))
            {
                lastMatch = match;
                if (foundNeedle)
                {
                    *((const char **)foundNeedle) = ((const char *)anyOfThese) + j;
                }
                r = ((char *)match) - ((char *)(buf + i));
            }
        }

        /* No need to wait. As the peek succeeded already, we should have all
         * bytes available already. */
        size_t r2 = Skip(t, r + (lastMatch ? 1 : 0), 0);
        KISO_UNUSED(r2);
        assert(r == r2);
    }
    if (lastMatch)
    {
        *len = i;
        return RETCODE_OK;
    }
    else
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
    }
}

static Retcode_T SkipUntil(struct AtTransceiver_S *t, const void *anyOfThese, const void **foundNeedle, TickType_t *timeout)
{
    uint8_t dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
    Retcode_T rc;
    do
    {
        size_t len = ATTRANSCEIVER_DUMMYBUFFERSIZE;
        rc = PopUntil(t, dummy, &len, anyOfThese, foundNeedle, timeout);
    } while (RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES) == rc);
    return rc;
}

Retcode_T AtTransceiver_Initialize(struct AtTransceiver_S *t,
                                   void *rxBuffer, size_t rxLength,
                                   AtTransceiver_WriteFunction_T writeFunc)
{
    RingBuffer_Initialize(&t->RxRingBuffer, (uint8_t *)rxBuffer, rxLength);

    t->Write = writeFunc;

    t->RxWakeupHandle = xSemaphoreCreateBinaryStatic(&t->RxWakeupBuffer);
    assert(NULL != t->RxWakeupHandle); /* due to static allocation */

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
    size_t r = 0;
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
            r = Skip(t, t->TxBufferUsed, &timeout);
            if (!r)
            {
                rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
            }
        }
        else
        {
            char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
            size_t totalRead = 0;
            do
            {
                r = Pop(t, dummy, ATTRANSCEIVER_MIN(t->TxBufferUsed - totalRead, sizeof(dummy)), &timeout);
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
    size_t r = Skip(t, length, &timeout);
    if (r < length)
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    }
    else
    {
        return RETCODE_OK;
    }
}

Retcode_T AtTransceiver_SkipArgument(struct AtTransceiver_S *t, TickType_t timeout)
{
    const void *needle = NULL;
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &needle, &timeout);

    if (RETCODE_OK == rc)
    {
        UpdateStartOfLineState(t, needle);
    }

    return rc;
}

Retcode_T AtTransceiver_SkipLine(struct AtTransceiver_S *t, TickType_t timeout)
{
    const void *needle = NULL;
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_S4, &needle, &timeout);

    if (RETCODE_OK == rc)
    {
        UpdateStartOfLineState(t, needle);
    }

    return rc;
}

Retcode_T AtTransceiver_ReadCommandAny(struct AtTransceiver_S *t, char *str, size_t length, TickType_t timeout)
{
    /* A singular response typically looks like this:
     * <S3><S4>+<command>:<attribute_list><S3><S4>
     */
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_MNEMONICSTART, NULL, &timeout);
    if (RETCODE_OK != rc)
        return rc;
    /* <S3><S4>+|<command>:<attribute_list><S3><S4>
     *          ^ We should be here now.
     */

    size_t r = length;
    rc = PopUntil(t, str, &r, ATTRANSCEIVER_ARGLIST, NULL, &timeout);
    if (RETCODE_OK == rc)
    {
        /* <S3><S4>+<command>:|<attribute_list><S3><S4>
         *                    ^ We should be here now.
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
        rc = SkipUntil(t, ATTRANSCEIVER_ARGLIST, NULL, &timeout);
        if (RETCODE_OK == rc)
        {
            /* <S3><S4>+<command>:|<attribute_list><S3><S4>
             *                    ^ We should be here now
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
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_MNEMONICSTART, NULL, &timeout);
    if (RETCODE_OK != rc)
        return rc;
    /* <S3><S4>+|<command>:<attribute_list><S3><S4>
     *          ^ We should be here now.
     */

    size_t len = strlen(str);
    size_t i;
    size_t r;
    for (i = 0; i < len; i += r)
    {
        char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE];
        r = Pop(t, dummy, ATTRANSCEIVER_MIN(sizeof(dummy), len - i), &timeout);
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
     *                   ^ We should be here now.
     * Meaning we still have to clean up the ':' before we leave.
     */

    if (str[i] != '\0')
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    else
        return SkipUntil(t, ATTRANSCEIVER_ARGLIST, NULL, &timeout); /* skip until beginning of attribute */
}

Retcode_T AtTransceiver_Read(struct AtTransceiver_S *t, void *data, size_t length, size_t *numActualBytesRead, TickType_t timeout)
{
    size_t r = Pop(t, data, length, &timeout);
    if (numActualBytesRead != NULL)
        *numActualBytesRead = r;
    if (r != length)
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    else
        return RETCODE_OK;
}

Retcode_T AtTransceiver_ReadArgument(struct AtTransceiver_S *t, char *str, size_t limit, TickType_t timeout)
{
    /* This function heavily relies on the zero-terminator space-reserve in str.
     * Without it, trimming trailing whitespace would be a bit more difficult
     * and would probably require a second (temporary) buffer. */
    Retcode_T rc = RETCODE_OK;
    if (0U >= limit)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    const void *needle = NULL;
    bool startOfArgument = false;
    bool endOfArgument = false;
    size_t offset = 0;
    size_t whitespaceBlockSize = 0;
    while (RETCODE_OK == rc && !endOfArgument)
    {
        size_t length = limit - offset;
        rc = PopUntil(t, str + offset, &length,
                      ATTRANSCEIVER_CONCAT4(ATTRANSCEIVER_WORDSEPARATOR,
                                            ATTRANSCEIVER_ARGSEPARATOR,
                                            ATTRANSCEIVER_S3,
                                            ATTRANSCEIVER_S4),
                      &needle, &timeout);

        bool foundWhitespace = NULL != needle &&
                               ((const char *)needle)[0] == ' ' &&
                               length <= 0;

        if (!startOfArgument && foundWhitespace)
        {
            /* We're still only trimming leading whitespace. */
            offset = 0;
        }
        else
        {
            startOfArgument = true;
            offset += length;

            endOfArgument = NULL != needle &&
                            ((const char *)needle)[0] != ' ';
            if (!endOfArgument && limit - offset > 1)
            {
                if (length)
                {
                    /* Seems like this time we popped multiple non-whitespace
                     * bytes. We therefore reset the whitespace counter. */
                    whitespaceBlockSize = 1;
                }
                else
                {
                    /* In case we encounter several consecutive whitespace
                     * characters, keep a count on 'em. We can't yet know if
                     * they are intermediate or trailing. If it turns out they
                     * are trailing, we can use this counter to reverse back to
                     * where the whitespace block started */
                    whitespaceBlockSize++;
                }
                str[offset++] = ' ';
            }
            else if (length)
            {
                /* Seems like we found the end of the argument. The fact that
                 * we have popped some bytes (indicated by length > 0) during
                 * our last venture, means that there weren't any trailing
                 * whitespace characters. */
                whitespaceBlockSize = 0;
            }
        }
    }

    if (RETCODE_OK == rc)
    {
        if (((const char *)needle)[0] == '\r')
        {
            (void)SkipUntil(t, ATTRANSCEIVER_S4, &needle, &timeout);
        }
        str[offset - whitespaceBlockSize] = '\0';

        UpdateStartOfLineState(t, needle);
    }

    return rc;
}

Retcode_T AtTransceiver_ReadString(struct AtTransceiver_S *t, char *str, size_t limit, TickType_t timeout)
{
    Retcode_T rc = RETCODE_OK;
    if (0U >= limit)
    {
        rc = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INVALID_PARAM);
    }

    if (RETCODE_OK == rc)
    {
        rc = SkipUntil(t, ATTRANSCEIVER_CONCAT3(ATTRANSCEIVER_STRLITERAL, ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), NULL, &timeout);
    }

    if (RETCODE_OK == rc)
    {
        rc = PopUntil(t, str, &limit, ATTRANSCEIVER_STRLITERAL, NULL, &timeout);
    }

    const void *needle = NULL;
    if (RETCODE_OK == rc)
    {
        str[limit] = '\0';
        rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &needle, &timeout); /* skip until end of attribute */
    }

    if (RETCODE_OK == rc)
    {
        UpdateStartOfLineState(t, needle);
    }

    return rc;
}

Retcode_T AtTransceiver_ReadHexString(struct AtTransceiver_S *t, void *data, size_t limit, size_t *numActualBytesRead, TickType_t timeout)
{
    Retcode_T rc = SkipUntil(t, ATTRANSCEIVER_CONCAT3(ATTRANSCEIVER_STRLITERAL, ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), NULL, &timeout);

    size_t r = 0;
    bool hitEnd = false;
    char dummy[ATTRANSCEIVER_DUMMYBUFFERSIZE * 2];
    for (size_t i = 0; i < limit && RETCODE_OK == rc && !hitEnd; i += r / 2, numActualBytesRead != NULL ? (*numActualBytesRead) += r / 2 : 0)
    {
        r = sizeof(dummy);
        rc = PopUntil(t, dummy, &r, ATTRANSCEIVER_STRLITERAL, NULL, &timeout);
        if (RETCODE_OK == rc)
        {
            hitEnd = true;
        }
        else if (RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES) == rc)
        {
            rc = RETCODE_OK; /* don't worry, we expected data to be too small to hold the full hex string. */
        }

        if (RETCODE_OK == rc)
        {
            rc = HexToBin(dummy, data + i, r / 2);
        }
    }

    const void *needle = NULL;
    if (RETCODE_OK == rc)
    {
        rc = SkipUntil(t, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &needle, &timeout); /* skip until end of attribute */
    }

    if (RETCODE_OK == rc)
    {
        UpdateStartOfLineState(t, needle);
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
    const void *needle = NULL;
    char buf[sizeof(ATTRANSCEIVER_TOSTRING(INT32_MAX)) + 1];
    size_t len = sizeof(buf) - 1;
    Retcode_T rc = PopUntil(t, buf, &len, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &needle, &timeout);
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

    UpdateStartOfLineState(t, needle);

    return RETCODE_OK;
}
Retcode_T AtTransceiver_ReadU32(struct AtTransceiver_S *t, uint32_t *x, int base, TickType_t timeout)
{
    const void *needle = NULL;
    char buf[sizeof(ATTRANSCEIVER_TOSTRING(INT32_MAX)) + 1];
    size_t len = sizeof(buf) - 1;
    Retcode_T rc = PopUntil(t, buf, &len, ATTRANSCEIVER_CONCAT(ATTRANSCEIVER_ARGSEPARATOR, ATTRANSCEIVER_S4), &needle, &timeout);
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

    UpdateStartOfLineState(t, needle);

    return RETCODE_OK;
}

Retcode_T AtTransceiver_ReadCode(struct AtTransceiver_S *t, enum AtTransceiver_ResponseCode_E *code, TickType_t timeout)
{

    /* Due to the way arguments are consumed, we can't be sure where exactly we
     * stand in the response code. Depending on preceeding argument handling,
     * our response may look like this:
     * |<S3><S4><verbose_code><S3><S4>
     * ^ We could be here.
     *
     * Or like this:
     * <S3><S4>|<verbose_code><S3><S4>
     *         ^ We could here.
     *
     * To deal with this the following implementation is fairly lax. We simply
     * consume "empty" lines (i.e. sequences of <S3><S4> with nothing in between)
     */

    char dummy[sizeof("INVALID COMMAND LINE")]; /* size of biggest verbose response code + '\0' */
    size_t len;
    Retcode_T rc = RETCODE_OK;
    for (unsigned int i = 0; i < ATTRANSCEIVER_SKIPEMPTYLINESLIMIT && RETCODE_OK == rc; ++i)
    {
        len = sizeof(dummy);
        rc = PopUntil(t, dummy, &len, ATTRANSCEIVER_S3, NULL, &timeout);
        if (len <= 0U)
        {
            /* We assume the next character is S4. Skip it! */
            (void)Skip(t, 1U, &timeout);
        }
        else
        {
            break;
        }
    }

    if (RETCODE_OK != rc)
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
        return SkipUntil(t, ATTRANSCEIVER_S4, NULL, &timeout);
    }
    else
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_INCONSISTENT_STATE);
    }
}

Retcode_T AtTransceiver_CheckEndOfLine(struct AtTransceiver_S *t, bool *eol, TickType_t timeout)
{
    char dummy[2];
    assert(sizeof(dummy) >= strlen(ATTRANSCEIVER_S3S4));

    size_t numRead = Peek(t, dummy, sizeof(dummy), &timeout);

    if (numRead < sizeof(dummy))
    {
        return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_TIMEOUT);
    }
    else
    {
        if (eol)
            *eol = 0 == memcmp(ATTRANSCEIVER_S3S4, dummy, sizeof(dummy));
        return RETCODE_OK;
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
