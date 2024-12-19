#include "SerialGPS.h"
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

void SerialGPS::processSentence(uint8_t *sentence, uint8_t size)
{
    // Parse NMEA sentence
    if (size < 6) {
        return;
    }

    // GPGGA contains lat, long, number of satellites, and altitude
    if (sentence[3] == 'G' && sentence[4] == 'G' && sentence[5] == 'A') {
        char *ptr = (char*)sentence;
        // Skip the "$GPGGA," part
        ptr = strchr(ptr, ',') + 1;

        // Parse time
        ptr = strchr(ptr, ',') + 1;

        // Parse lat
        if (ptr != NULL)
        {
            gpsData.lat = strtof(ptr, NULL) / 100;
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse lat direction
        if (ptr != NULL)
        {
            if (*ptr == 'S')
            {
                gpsData.lat = -gpsData.lat;
            }
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse lon
        if (ptr != NULL)
        {
            gpsData.lon = strtof(ptr, NULL) / 100;
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse lon direction
        if (ptr != NULL)
        {
            if (*ptr == 'W')
            {
                gpsData.lon = -gpsData.lon;
            }
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse fix quality
        ptr = strchr(ptr, ',') + 1;

        // Parse number of satellites
        if (ptr != NULL)
        {
            gpsData.satellites = atoi(ptr);
        }
        ptr = strchr(ptr, ',') + 1;

        // Parse HDOP
        ptr = strchr(ptr, ',') + 1;

        // Parse altitude
        if (ptr != NULL)
        {
            gpsData.alt = strtof(ptr, NULL);
        }
        ptr = strchr(ptr, ',') + 1;
    } else if (sentence[3] == 'V' && sentence[4] == 'T' && sentence[5] == 'G') {
        char *ptr = (char*)sentence;

        // Skip the "$GNVTG," part
        ptr = strchr(ptr, ',') + 1;

        // Parse the heading, if present
        if (ptr != NULL && *ptr != ',')
        {
            gpsData.heading = strtof(ptr, NULL);
        }
        ptr = strchr(ptr, ',') + 1;
        // Parse the next token (T)
        ptr = strchr(ptr, ',') + 1;
        // Parse the next tokens (track + M)
        ptr = strchr(ptr, ',') + 1;
        ptr = strchr(ptr, ',') + 1;
        // Parse the speed over ground in knots
        ptr = strchr(ptr, ',') + 1;
        ptr = strchr(ptr, ',') + 1;
        // Parse the speed over ground in km/h if present
        if (ptr != NULL && *ptr != ',')
        {
            gpsData.speed = strtof(ptr, NULL);
        }
    }
}

void SerialGPS::processBytes(uint8_t *bytes, uint16_t size)
{
    // Buffer NMEA data; parse only lat/lon/alt/speed
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
    crsfgps.p.latitude = htobe32((int32_t)(gpsData.lat * 1e7));
    crsfgps.p.longitude = htobe32((int32_t)(gpsData.lon * 1e7));
    crsfgps.p.altitude = htobe16((int16_t)(gpsData.alt + 1000));
    crsfgps.p.groundspeed = htobe16((uint16_t)(gpsData.speed * 10));
    crsfgps.p.satellites_in_use = gpsData.satellites;
    crsfgps.p.gps_heading = htobe16((uint16_t)gpsData.heading * 100);
    CRSF::SetHeaderAndCrc((uint8_t *)&crsfgps, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    telemetry.AppendTelemetryPackage((uint8_t *)&crsfgps);
}