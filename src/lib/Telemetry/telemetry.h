#pragma once

#define TELEMETRY_LENGTH_INDEX 1
#define TELEMETRY_TYPE_INDEX 2
#define TELEMETRY_ADDITIONAL_LENGTH 2
#define TELEMETRY_CRC_LENGTH 1
#define CRSF_MAX_PACKET_LEN 64
#define CRSF_SYNC_BYTE 0xC8
#define CRSF_FRAMETYPE_COMMAND 0x32
#define CRSF_ADDRESS_CRSF_RECEIVER 0xEC

typedef enum {
    TELEMTRY_IDLE = 0,
    RECEIVING_LEGNTH,
    RECEIVING_DATA
} telemetry_state_s;

typedef struct crsf_telemetry_package_t {
    char *data;
    volatile bool locked;
    volatile struct crsf_telemetry_package_t* next;
} crsf_telemetry_package_t;

enum {
    CRSF_FRAME_GPS_PAYLOAD_SIZE = 15,
    CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE = 8,
    CRSF_FRAME_ATTITUDE_PAYLOAD_SIZE = 6,
};

class Telemetry
{
public:
    bool RXhandleUARTin(char data);

private:
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    void AppendTelemetryPackage();
    static telemetry_state_s telemtry_state;
    static char CRSFinBuffer[CRSF_MAX_PACKET_LEN + TELEMETRY_CRC_LENGTH];
    static char currentTelemetryByte;
    static volatile crsf_telemetry_package_t *telemetryPackageHead;
};