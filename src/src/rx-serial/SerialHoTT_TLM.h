#pragma once

#include "SerialIO.h"
#include "device.h"

#define PACKED __attribute__((packed))

#define HOTT_MAX_BUF_LEN 64    // max buffer size for serial in data

#define FRAME_SIZE 45          // HoTT telemetry frame size
#define CMD_LEN 2              // HoTT poll command length
#define STARTBYTE_INDEX 0      // index of start byte
#define DEVICE_INDEX 1         // index of device ID
#define ENDBYTE_INDEX 43       // index of end byte
#define CRC_INDEX  44          // index of CRC

#define START_FRAME_B 0x7C     // HoTT start of frame marker
#define END_FRAME 0x7D         // HoTT end of frame marker

#define START_OF_CMD_B 0x80    // start byte of HoTT binary cmd sequence
#define SENSOR_ID_GPS_B 0x8A   // device ID binary mode GPS module
#define SENSOR_ID_GPS_T 0xA0   // device ID for text mode adressing
#define SENSOR_ID_GAM_B 0x8D   // device ID binary mode GAM module
#define SENSOR_ID_GAM_T 0xD0   // device ID for text mode adressing
#define SENSOR_ID_EAM_B 0x8E   // device ID binary mode EAM module
#define SENSOR_ID_EAM_T 0xE0   // device ID for text mode adressing
#define SENSOR_ID_ESC_B 0x8C   // device ID binary mode ESC module
#define SENSOR_ID_ESC_T 0xC0   // device ID for text mode adressing
#define SENSOR_ID_VARIO_B 0x89 // device ID binary mode VARIO module
#define SENSOR_ID_VARIO_T 0x90 // device ID for text mode adressing

//
// GAM data frame data structure
//
typedef struct
{
    uint8_t startByte = START_FRAME_B;        //  0 0x7C
    uint8_t packetId = SENSOR_ID_GAM_B;       //  1 0x8D HOTT_General_AIR_PACKET_ID
    uint8_t warnBeep = 0;                     //  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
    uint8_t packetIdText = SENSOR_ID_GAM_T;   //  3 0xD0 Sensor ID text mode
    uint8_t inverse1 = 0;                     //  4 Alarm_inverse_1
    uint8_t inverse2 = 0;                     //  5 Alarm_inverse_2
    uint8_t voltageCell1 = 0;                 //  6 124 = 2,48V (voltage*50)
    uint8_t voltageCell2 = 0;                 //  7 132 = 2,64V
    uint8_t voltageCell3 = 0;                 //  8
    uint8_t voltageCell4 = 0;                 //  9
    uint8_t voltageCell5 = 0;                 // 10
    uint8_t voltageCell6 = 0;                 // 11
    uint16_t battery1 = 0;                    // 12 51 = 5,1V Batt1 voltage
    uint16_t battery2 = 0;                    // 14 51 = 5,1V Batt2 voltage
    uint8_t temperature1 = 20;                // 16 46 = 26C, 0 = -20C (temp+20)
    uint8_t temperature2 = 20;                // 17 45 = 25C, 0 = -20C (temp+20)
    uint8_t fuelScale = 0;                    // 18 0..100 (scales to display 0/25/50/75/100 increments)
    uint16_t fuel = 0;                        // 19 65535 = full
    uint16_t rpm1 = 0;                        // 21 (rpm/10)
    uint16_t altitude = 500;                  // 23 500 = 0m (altitude+500)
    uint16_t mPerSec = 30000;                 // 25 1 = 0,01m/s 30000 = 0m/s (climb rate/100 + 30000)
    uint8_t mPer3sec = 120;                   // 27 120 = 0m per 3 sek
    uint16_t current = 0;                     // 28 1 = 0.1A (current*10)
    uint16_t inputVoltage = 0;                // 30 166 = 16,6V (voltage*10)
    uint16_t capacity = 0;                    // 32 1 = 10mAh (capacity in mAh/10)
    uint16_t speed = 0;                       // 34 1 = 2km/h (speed/2)
    uint8_t lowCellVoltage = 0;               // 36 124 = 2,48V (voltage*50)
    uint8_t cellNumberLcv = 0;                // 37
    uint16_t rpm2 = 0;                        // 38 (rpm/10)
    uint8_t generalError = 0;                 // 40 general error number (voice error = 12)
    uint8_t pressure = 0;                     // 41 pressure 20 = 2bar (pressure*10)
    uint8_t version = 0;                      // 42 version number
    uint8_t endByte = END_FRAME;              // 43 0x7D
    uint8_t crc = 0x00;                       // 44 CRC
} PACKED GeneralAirPacket_t;

//
// GPS data frame data structure
//
typedef struct
{
    uint8_t startByte = START_FRAME_B;        //  0 0x7C
    uint8_t packetId = SENSOR_ID_GPS_B;       //  1 0x8A HOTT_GPS_PACKET_ID
    uint8_t warnBeep = 0;                     //  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
    uint8_t packetIdText = SENSOR_ID_GPS_T;   //  3 0xA0 Sensor ID text mode
    uint8_t alarmInverse = 0;                 //  4 0 Inverse status
    uint8_t GpsInverse = 1;                   //  5 1 = no GPS signal
    uint8_t direction = 0;                    //  6 1 = 2 degrees; 0 = N, 90 = E, 180 = S, 270 = W
    uint16_t speed = 0;                       //  7 1 = 1 km/h
    uint8_t latNS = 0;                        //  9 example: N48D39'0988'', 0 = N
    uint16_t latDegMin = 0;                   // 10 48D39' = 4839 = 0x12e7
    uint16_t latSec = 0;                      // 12 0988'' = 988 = 0x03DC
    uint8_t lonEW = 0;                        // 14 example: E09D25'9360'', 0 = E
    uint16_t lonDegMin = 0;                   // 15 09D25' = 0925 = 0x039D
    uint16_t lonSec = 0;                      // 17 9360'' = 9360 = 0x2490
    uint16_t distance = 0;                    // 19 1 = 1m
    uint16_t altitude = 500;                  // 21 500 = 0m
    uint16_t mPerSec = 30000;                 // 23 30000 = 0.00m/s (1 = 0.01m/s)
    uint8_t mPer3sec = 120;                   // 25 120 = 0m/3s (1 = 1m/3s)
    uint8_t satellites = 0;                   // 26 n visible satellites
    uint8_t fixChar = ' ';                    // 27 GPS fix character. Display if DGPS, 2D oder 3D
    uint8_t homeDir = 0;                      // 28 GPS home direction 1 = 2 degreed
    int8_t roll = 0;                          // 29 signed roll angle 1 = 2 degrees
    int8_t pitch = 0;                         // 30 signed pitch angle 1 = 2 degrees
    int8_t Yaw = 0;                           // 31 signed yaw angle 1 = 2 degrees
    uint8_t timeHours = 0;                    // 32 GPS time hours
    uint8_t timeMinutes = 0;                  // 33 GPS time minutes
    uint8_t timeSeconds = 0;                  // 34 GPS time seconds
    uint8_t timeHundreds = 0;                 // 35 GPS time 1/100 seconds
    uint16_t mslAltitude = 0;                 // 36 1 = 1m
    uint8_t vibrations = 0;                   // 38 vibration level in %
    uint8_t ascii1 = '-';                     // 39 free ASCII character 1
    uint8_t ascii2 = ' ';                     // 40 free ASCII character 2
    uint8_t ascii3 = '-';                     // 41 free ASCII character 3
    uint8_t version = 0;                      // 42 version number
    uint8_t endByte = END_FRAME;              // 43 0x7D
    uint8_t crc = 0x00;                       // 44 CRC
} PACKED GPSPacket_t;

//
// EAM data frame data structure
//
typedef struct
{
    uint8_t startByte = START_FRAME_B;        //  0 0x7C
    uint8_t packetId = SENSOR_ID_EAM_B;       //  1 0x8E HOTT_Electric_Air_ID
    uint8_t warnBeep = 0;                     //  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
    uint8_t packetIdText = SENSOR_ID_EAM_T;   //  3 0xE0 Sensor ID text mode
    uint8_t inverse1 = 0;                     //  4 alarm bitmask. Value is displayed inverted
                                              // Bit#  Alarm field
                                              // 0    mAh
                                              // 1    Battery 1
                                              // 2    Battery 2
                                              // 3    Temperature 1
                                              // 4    Temperature 2
                                              // 5    Altitude
                                              // 6    Current
                                              // 7    Main power voltage
    uint8_t inverse2 = 0;                     //  5 alarm bitmask. Value is displayed inverted
                                              // Bit#  Alarm Field
                                              // 0    m/s
                                              // 1    m/3s
                                              // 2    Altitude (duplicate?)
                                              // 3    m/s (duplicate?)
                                              // 4    m/3s (duplicate?)
                                              // 5    unknown/unused
                                              // 6    unknown/unused
                                              // 7    "ON" sign/text msg active
    uint8_t cellL[7] = {0};                   // 06 cell 1 voltage lower value. 0.02V steps, 124=2.48V
    uint8_t cellH[7] = {0};                   // 13 cell 1 voltage high value. 0.02V steps, 124=2.48V
    uint16_t battVoltage1 = 0;                // 20 battery 1 voltage lower value in 100mv steps, 50=5V. optionally cell8_L value 0.02V steps
    uint16_t battVoltage2 = 0;                // 22 battery 2 voltage lower value in 100mv steps, 50=5V. optionally cell8_H value. 0.02V steps
    uint8_t temp1 = 20;                       // 24 Temperature sensor 1. 20=0�, 46=26� - offset of 20.
    uint8_t temp2 = 20;                       // 25 temperature sensor 2
    uint16_t altitude = 500;                  // 26 Altitude lower value. unit: meters. Value of 500 = 0m
    uint16_t current = 0;                     // 28 Current in 0.1 steps
    uint16_t mainVoltage = 0;                 // 30 Main power voltage (drive) in 0.1V steps
    uint16_t capacity = 0;                    // 32 used battery capacity in 10mAh steps
    uint16_t mPerSec = 30000;                 // 34 climb rate in 0.01m/s. Value of 30000 = 0.00 m/s
    uint8_t mPer3sec = 120;                   // 36 climbrate in m/3sec. Value of 120 = 0m/3sec
    uint16_t rpm = 0;                         // 37 RPM. Steps: 10 U/min
    uint8_t electricMin = 0;                  // 39 Electric minutes. Time does start, when motor current is > 3 A
    uint8_t electricSec = 0;                  // 40 Electric seconds.
    uint16_t speed = 0;                       // 41 speed in km/h. Steps 1km/h
    uint8_t endByte = END_FRAME;              // 43 0x7D
    uint8_t Crc;                              // 44 CRC
} PACKED ElectricAirPacket_t;

//
// ESC data frame data structure
//
typedef struct HOTT_AIRESC_MSG_s
{
    uint8_t startByte = START_FRAME_B;        //  0 0x7C
    uint8_t packetId = SENSOR_ID_ESC_B;       //  1 0x8C HOTT_General_AIR_PACKET_ID
    uint8_t warnBeep = 0;                     //  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
    uint8_t packetIdText = SENSOR_ID_ESC_T;   //  3 0xC0 Sensor ID text mode
    uint8_t inverse1 = 0;                     //  4 TODO: more info
    uint8_t inverse2 = 0;                     //  5 TODO: more info
    uint16_t inputVoltage = 0;                //  6 Input voltage
    uint16_t inputVoltageMin = 0;             //  8 Input min. voltage
    uint16_t capacity = 0;                    // 10 battery capacity in 10mAh steps
    uint8_t escTemp = 0;                      // 12 ESC temperature
    uint8_t escMaxTemp = 0;                   // 13 ESC max. temperature
    uint16_t current = 0;                     // 14 Current in 0.1 steps
    uint16_t currentMax = 0;                  // 16 Current max. in 0.1 steps
    uint16_t rpm = 0;                         // 18 RPM in 10U/min steps
    uint16_t rpmMax = 0;                      // 20 RPM max
    uint8_t motorTemp = 0;                    // 22 motor temp
    uint8_t motortempMax = 0;                 // 23 motor temp max
    uint16_t speed = 0;                       // 24 Speed
    uint16_t speedMax = 0;                    // 26 Speed max
    uint8_t pwm = 0;                          // 28 PWM in %
    uint8_t throttle = 0;                     // 29 throttle in %
    uint8_t becVoltage = 0;                   // 30 BEC voltage
    uint8_t becVoltageMin = 0;                // 31 BEC voltage min
    uint8_t becCurrent = 0;                   // 32 BEC current
    uint8_t becTemp = 0;                      // 34 BEC temperature
    uint8_t capacitorTemp = 0;                // 35 Capacitor temperature
    uint8_t motorTiming = 0;                  // 36 motor timing
    uint8_t auxTemp = 0;                      // 37 advanced timing/aux temp
    uint16_t thrust = 0;                      // 38 thrust/force
    uint8_t pumpTemp = 0;                     // 40 pump temperature
    uint8_t turbineNumber = 0;                // 41 engine number (200 = #1, 201 = #2)
    uint8_t version = 3;                      // 42 version number = 3 for new protocol
    uint8_t endByte = END_FRAME;              // 43 0x7D
    uint8_t crc;                              // 44 CRC
} PACKED AirESCPacket_t;

//
// VARIO data frame data structure
//
typedef struct HOTT_VARIO_MSG_s
{
    uint8_t startByte = START_FRAME_B;          //  0 0x7C
    uint8_t packetId = SENSOR_ID_VARIO_B;       //  1 0x89 HOTT_Vario_PACKET_ID
    uint8_t warnBeep = 0;                       //  2 warn beep (0 = no beep, 0x00..0x1A warn beeps)
    uint8_t packetIdText = SENSOR_ID_VARIO_T;   //  3 0x90 Sensor ID text mode
    uint8_t inverse1 = 0;                       //  4 Inverse display (alarm?) bitmask
    uint16_t altitude = 500;                    //  5 Altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t altitudeMax = 500;                 //  7 Max. measured altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t altitudeMin = 500;                 //  9 Min. measured altitude low uint8_t. In meters. A value of 500 means 0m
    uint16_t mPerSec = 30000;                   // 11 Climb rate in m/s. Steps of 0.01m/s. Value of 30000 = 0.00 m/s
    uint16_t mPerSec3s = 30000;                 // 13 Climb rate in m/3s. Steps of 0.01m/3s. Value of 30000 = 0.00 m/3s
    uint16_t mPerSec10s = 30000;                // 15 Climb rate m/10s. Steps of 0.01m/10s. Value of 30000 = 0.00 m/10s
    uint8_t textMsg[21];                        // 17 Free ASCII text message
    uint8_t freeChar1 = ' ';                    // 38 Free ASCII character.  appears right to home distance
    uint8_t freeChar2 = ' ';                    // 39 Free ASCII character.  appears right to home direction
    uint8_t freeChar3 = ' ';                    // 40 Free ASCII character.  appears? TODO: Check where this char appears
    uint8_t compassDir = 0;                     // 41 Compass heading in 2 degrees steps. 1 = 2 degrees
    uint8_t version = 0;                        // 42 version number = 3 for new protocol
    uint8_t endByte = END_FRAME;                // 43 0x7D
    uint8_t crc;                                // 44 CRC
} PACKED VarioPacket_t;

typedef struct
{
    uint8_t cmdMirror[CMD_LEN];
    uint8_t payload[FRAME_SIZE];
} PACKED hottBusFrame_t;

enum HoTTDevices
{
    FIRST_DEVICE = 0,
    GPS = FIRST_DEVICE,
    EAM,
    GAM,
    ESC,
    VARIO,
    LAST_DEVICE
};

typedef struct
{
    uint8_t deviceID;
    bool present;
} hottDevice_t;


class SerialHoTT_TLM : public SerialIO
{
public:
    explicit SerialHoTT_TLM(Stream &out, Stream &in)
        : SerialIO(&out, &in)
    {
        uint32_t now = millis();

        lastPoll = now;
        discoveryTimerStart = now;
    }

    virtual ~SerialHoTT_TLM() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t *data) override {}
    uint32_t sendRCFrame(bool frameAvailable, uint32_t *channelData) override { return DURATION_IMMEDIATELY; };

    int getMaxSerialReadSize() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;

    void pollNextDevice();
    void pollDevice(uint8_t id);
    void processFrame();
    uint8_t calcFrameCRC(uint8_t *buf);

    void scheduleCRSFtelemetry(uint32_t now);
    void sendCRSFvario(uint32_t now);
    void sendCRSFgps(uint32_t now);
    void sendCRSFbattery(uint32_t now);

    uint16_t getHoTTvoltage();
    uint16_t getHoTTcurrent();
    uint32_t getHoTTcapacity();
    int16_t getHoTTaltitude();
    int16_t getHoTTvv();
    uint8_t getHoTTremaining();
    int32_t getHoTTlatitude();
    int32_t getHoTTlongitude();
    uint16_t getHoTTgroundspeed();
    uint16_t getHoTTheading();
    uint8_t getHoTTsatellites();
    uint16_t getHoTTMSLaltitude();

    uint32_t htobe24(uint32_t val);

    // last received HoTT telemetry packets
    GPSPacket_t gps;
    GeneralAirPacket_t gam;
    AirESCPacket_t esc;
    VarioPacket_t vario;
    ElectricAirPacket_t eam;

    // received HoTT bus fra,e
    hottBusFrame_t hottBusFrame;

    // discoverd devices
    hottDevice_t device[LAST_DEVICE] = {
        {SENSOR_ID_GPS_B, false},
        {SENSOR_ID_EAM_B, false},
        {SENSOR_ID_GAM_B, false},
        {SENSOR_ID_ESC_B, false},
        {SENSOR_ID_VARIO_B, false}};

    FIFO<HOTT_MAX_BUF_LEN> hottInputBuffer;

    bool discoveryMode = true;
    uint8_t nextDevice = FIRST_DEVICE;

    uint32_t lastPoll;
    uint32_t discoveryTimerStart;

    uint32_t lastVarioSent = 0;
    uint32_t lastVarioCRC = 0;
    uint32_t lastGPSSent = 0;
    uint32_t lastGPSCRC = 0;
    uint32_t lastBatterySent = 0;
    uint32_t lastBatteryCRC = 0;

    const uint8_t DegMinScale = 100;
    const uint8_t SecScale = 100;
    const uint8_t MinDivide = 6;
    const int32_t MinScale = 1000000L;
    const int32_t DegScale = 10000000L;
};
