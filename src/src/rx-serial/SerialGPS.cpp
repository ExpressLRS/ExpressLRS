#include "SerialGPS.h"

#include "CRSFEndpoint.h"
#include "msptypes.h"
#include <crsf_protocol.h>
#include <telemetry.h>

extern Telemetry telemetry;

void SerialGPS::sendQueuedData(uint32_t maxBytesToSend)
{
    sendTelemetryFrame();
}

void SerialGPS::queueMSPFrameTransmission(uint8_t* data)
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
        for (int i = 0; i < scaleDecimals && dec[i]; i++) {
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

void SerialGPS::processSentence(uint8_t *sentence, uint8_t size)
{
    if (size < 6) {
        return;
    }

    if (sentence[3] == 'G' && sentence[4] == 'G' && sentence[5] == 'A') {
        char *ptr = (char*)sentence;
        ptr = strchr(ptr, ',') + 1;
        ptr = strchr(ptr, ',') + 1;

        // Parse lat
        if (ptr != NULL) {
            int32_t degrees = atoi(ptr) / 100;
            char minutes[20];
            strncpy(minutes, ptr + 2, 19);
            int32_t minutesPart = parseDecimalToScaled(minutes, 10000000) / 60;
            
            gpsData.lat = degrees * 10000000 + minutesPart;
        }
        ptr = strchr(ptr, ',') + 1;

        if (ptr != NULL && *ptr == 'S') {
            gpsData.lat = -gpsData.lat;
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse lon - similar to lat
        if (ptr != NULL) {
            int32_t degrees = atoi(ptr) / 100;
            char minutes[20];
            strncpy(minutes, ptr + 2, 19);
            int32_t minutesPart = parseDecimalToScaled(minutes, 10000000) / 60;
            
            gpsData.lon = degrees * 10000000 + minutesPart;
        }
        ptr = strchr(ptr, ',') + 1;

        if (ptr != NULL && *ptr == 'W') {
            gpsData.lon = -gpsData.lon;
        }
        ptr = strchr(ptr, ',') + 1;

        ptr = strchr(ptr, ',') + 1;

        if (ptr != NULL) {
            gpsData.satellites = atoi(ptr);
        }
        ptr = strchr(ptr, ',') + 1;
        ptr = strchr(ptr, ',') + 1;

        // Parse altitude into centimeters
        if (ptr != NULL) {
            gpsData.alt = parseDecimalToScaled(ptr, 100);
        }
    }
    else if (sentence[3] == 'V' && sentence[4] == 'T' && sentence[5] == 'G') {
        char *ptr = (char*)sentence;
        ptr = strchr(ptr, ',') + 1;

        // Parse heading (into degrees * 100)
        if (ptr != NULL && *ptr != ',') {
            gpsData.heading = parseDecimalToScaled(ptr, 100);
        }

        // Skip to speed
        for (int i = 0; i < 6; i++) {
            ptr = strchr(ptr, ',') + 1;
        }

        // Parse speed (into km/h * 100)
        if (ptr != NULL && *ptr != ',') {
            gpsData.speed = parseDecimalToScaled(ptr, 100);
        }
    }
}

void SerialGPS::processBytes(uint8_t *bytes, uint16_t size)
{
    static uint8_t nmeaBuffer[128];
    static uint8_t nmeaBufferIndex = 0;

    for (uint16_t i = 0; i < size; i++) {
        if (nmeaBufferIndex < sizeof(nmeaBuffer)) {
            nmeaBuffer[nmeaBufferIndex++] = bytes[i];
        }
        if (bytes[i] == '\n') {
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
    crsfgps.p.gps_heading = htobe16((uint16_t)gpsData.heading);
    crsfEndpoint->SetHeaderAndCrc((crsf_header_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    crsfEndpoint->processMessage(nullptr, &crsfgps.h);
}