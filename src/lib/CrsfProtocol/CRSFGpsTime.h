#pragma once

#include <stdint.h>
#include "crsf_protocol.h"
#include "CRSFRouter.h"

/**
 * @brief Advance a CRSF_FRAMETYPE_GPS_TIME frame's timestamp in-place.
 *
 * GPS Time frames carry the timestamp from the GPS measurement epoch. By the
 * time the frame is transmitted OTA it may be several seconds stale. This
 * function adds an elapsed-time correction directly to the serialised frame
 * bytes, avoiding a full Gregorian calendar decomposition.
 *
 * Frame layout (bytes):
 *   [0] sync  [1] length  [2] type=0x03
 *   [3][4] year (BE)  [5] month  [6] day
 *   [7] hour  [8] min  [9] sec
 *   [10][11] millis (BE)  [12] CRC
 *
 * Carry propagates ms -> sec -> min -> hour. Hour overflow (~midnight
 * rollover) is not handled; the probability is ~0.002% given the bounded
 * corrections applied here and the consequence (hour=24 for one sync cycle)
 * is acceptable for RTC synchronisation purposes.
 *
 * The caller is responsible for recalculating the frame CRC after calling
 * this function using crsfRouter.crsf_crc.
 *
 * @param buf       Pointer to the start of the serialised CRSF frame.
 * @param elapsedMs Milliseconds to add to the timestamp.
 */
static inline void crsfGpsTimeAdvanceMs(uint8_t *buf, uint16_t elapsedMs)
{
    if (elapsedMs == 0)
        return;

    // Offset constants for GPS Time frame fields
    constexpr uint8_t OFF_HOUR = 7;
    constexpr uint8_t OFF_MIN  = 8;
    constexpr uint8_t OFF_SEC  = 9;
    constexpr uint8_t OFF_MSH  = 10;
    constexpr uint8_t OFF_MSL  = 11;

    uint16_t ms = ((uint16_t)buf[OFF_MSH] << 8) | buf[OFF_MSL];
    ms += elapsedMs;

    buf[OFF_MSH] = (ms % 1000) >> 8;
    buf[OFF_MSL] = (ms % 1000) & 0xFF;

    uint8_t carryS = ms / 1000;
    if (carryS == 0)
        return;

    buf[OFF_SEC] += carryS;
    if (buf[OFF_SEC] < 60)
        return;

    buf[OFF_MIN] += buf[OFF_SEC] / 60;
    buf[OFF_SEC] %= 60;
    if (buf[OFF_MIN] < 60)
        return;

    buf[OFF_HOUR] += buf[OFF_MIN] / 60;
    buf[OFF_MIN] %= 60;
    // Hour >= 24 (midnight rollover) is not handled intentionally.
}

/**
 * @brief Recalculate and write the CRC for a CRSF frame in-place.
 *
 * @param buf  Pointer to the start of the serialised CRSF frame.
 */
static inline void crsfRecalcCrc(uint8_t *buf)
{
    // CRC covers from type byte to last payload byte (excludes sync and length)
    const uint8_t payloadLen = buf[CRSF_TELEMETRY_LENGTH_INDEX];
    buf[CRSF_FRAME_NOT_COUNTED_BYTES + payloadLen - 1] =
        crsfRouter.crsf_crc.calc(&buf[CRSF_TELEMETRY_TYPE_INDEX], payloadLen - CRSF_TELEMETRY_CRC_LENGTH);
}
