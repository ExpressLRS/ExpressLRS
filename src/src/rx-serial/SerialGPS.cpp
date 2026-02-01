#include "SerialGPS.h"

#include "CRSFRouter.h"
#include <crsf_protocol.h>

void SerialGPS::sendQueuedData(uint32_t maxBytesToSend)
{
}

// Parses a decimal string with optional decimal point and returns the value scaled by the given factor as an integer
// Ex: "0.442" with scale 100 returns 44
// Ex: "123.456" with scale 1000 returns 123456
int32_t parseDecimalToScaled(const char* str, int32_t scale) {
    char *end;
    int32_t whole = strtol(str, &end, 10);
    int32_t result = whole * scale;

    if (*end == '.') {
        const char* dec = end + 1;
        int32_t divisor = 1;
        int32_t decimalPart = 0;

        // Count decimal places in scale
        int32_t scaleDecimals = 0;
        int32_t tempScale = scale;
        while (tempScale > 1) {
            scaleDecimals++;
            tempScale /= 10;
        }

        // Process up to scaleDecimals digits
        for (int i = 0; i < scaleDecimals && dec[i] != '\0'; i++) {
            decimalPart = decimalPart * 10 + (dec[i] - '0');
            divisor *= 10;
        }

        // Scale the decimal part
        if (divisor > 1) {
            while (divisor < scale) {
                decimalPart *= 10;
                divisor *= 10;
            }
            result += decimalPart;
        }
    }
    return result;
}

/***
 * @brief Parse NMEA Decimal Degrees Minutes value and return Decimal Degrees * 1e7.
 * @param field Points to beginning of DDMM[M] string, must not be blank!
 */
int32_t nmeaDdmToDd(const char *field)
{
    // Latitude is DDMM.MMMMM, Longitude is DDDMM.MMMM
    // Start by getting the part before the decimal, and divide by 100 to remove the MM part
    int32_t degrees = atoi(field) / 100;

    // Minutes is always two digits. Find the decimal and look 2 characters before it
    char *minutes = strchr(field, '.');
    if (minutes == nullptr)
        return 0;
    int32_t minutesPart = parseDecimalToScaled(minutes - 2, 10000000) / 60;

    return degrees * 10000000 + minutesPart;
}

bool SerialGPS::isValidChecksum(char *sentence, uint8_t size)
{
    // Could also check for the \r\n but we know it at least has the \n to get here
    if (size < 6 || sentence[0] != '$' || sentence[size-5] != '*')
    {
        DBGLN("NMEA invalid");
        return false;
    }

    // Checksum in a NMEA packet starts after the $ and stops before the *XX and is a simple XOR
    uint8_t csumCalculated = 0;
    for (unsigned b=1; b<size-5; ++b)
    {
        csumCalculated ^= sentence[b];
    }

    uint8_t csumSentence = strtol((char *)&sentence[size-4], nullptr, 16);
    if (csumCalculated != csumSentence)
    {
        DBGLN("NMEA csum");
        return false;
    }

    return true;
}

void SerialGPS::splitSentenceFields(char *sentence, uint8_t size, gpsFieldParser_t callback)
{
    uint8_t fieldIdx = 0;
    char *fieldStart = sentence;

    //sentence[size] = 0; DBG(sentence);
    for (unsigned i=0; i<size; ++i)
    {
        if (sentence[i] == ',' || sentence[i] == '*')
        {
            sentence[i] = 0;
            callback(this, fieldIdx, fieldStart);
            fieldStart = &sentence[i+1];
            ++fieldIdx;
        }
    }
}

void SerialGPS::fieldParseGGA(SerialGPS *ctx, uint8_t fieldIdx, char *field)
{
    const bool blank = (field[0] == '\0');

    switch (fieldIdx)
    {
        case 2:
            ctx->gpsData.lat = (blank) ? 0 : nmeaDdmToDd(field);
            break;
        case 3:
            if (field[0] == 'S')
                ctx->gpsData.lat = -ctx->gpsData.lat;
            break;
        case 4:
            ctx->gpsData.lon = (blank) ? 0 : nmeaDdmToDd(field);
            break;
        case 5:
            if (field[0] == 'W')
                ctx->gpsData.lon = -ctx->gpsData.lon;
            break;
        case 7:
            ctx->gpsData.satellites = atoi(field);
            break;
        case 9:
            ctx->gpsData.alt = (blank) ? 0 : parseDecimalToScaled(field, 100);;
            break;

    }
}

void SerialGPS::fieldParseVTG(SerialGPS *ctx, uint8_t fieldIdx, char *field)
{
    const bool blank = (field[0] == '\0');

    switch (fieldIdx)
    {
        case 1:
            ctx->gpsData.heading = (blank) ? 0 : parseDecimalToScaled(field, 100);
            break;
        case 7:
            ctx->gpsData.speed = (blank) ? 0 : parseDecimalToScaled(field, 100);
            break;
    }
}

void SerialGPS::processSentence(char *sentence, uint8_t size)
{
    if (sentence[3] == 'G' && sentence[4] == 'G' && sentence[5] == 'A') {
        splitSentenceFields(sentence, size, &fieldParseGGA);
        sendTelemetryFrame();
    }
    else if (sentence[3] == 'V' && sentence[4] == 'T' && sentence[5] == 'G') {
        splitSentenceFields(sentence, size, &fieldParseVTG);
        // VTG usually comes before GGA, only generate one telemetry frame with both combined
        //sendTelemetryFrame();
    }
}

void SerialGPS::processBytes(uint8_t *bytes, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++) {
        char c = bytes[i];
        if (nmeaBufferIndex < sizeof(nmeaBuffer)) {
            nmeaBuffer[nmeaBufferIndex++] = c;
        }
        if (c == '\n') {
            // Note that the buffer/size includes the \r\n
            if (isValidChecksum(nmeaBuffer, nmeaBufferIndex))
                processSentence(nmeaBuffer, nmeaBufferIndex);
            nmeaBufferIndex = 0;
        }
    }
}

void SerialGPS::sendTelemetryFrame()
{
    CRSF_MK_FRAME_T(crsf_sensor_gps_t) crsfgps = { 0 };
    crsfgps.p.latitude = htobe32(gpsData.lat);
    crsfgps.p.longitude = htobe32(gpsData.lon);
    crsfgps.p.altitude = htobe16((int16_t)(gpsData.alt / 100 + 1000));
    crsfgps.p.groundspeed = htobe16((uint16_t)(gpsData.speed / 10));
    crsfgps.p.satellites_in_use = gpsData.satellites;
    crsfgps.p.gps_heading = htobe16(gpsData.heading);
    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfgps.h);
}