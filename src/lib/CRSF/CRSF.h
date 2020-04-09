#ifndef H_CRSF
#define H_CRSF

#include <Arduino.h>
#include "../../src/HwSerial.h"

#ifdef ARDUINO_ARCH_STM32
#define VOLATILE
#else
#define VOLATILE volatile
#endif

#define PACKED __attribute__((packed))

#define CRSF_RX_BAUDRATE 420000
#define CRSF_OPENTX_BAUDRATE 400000
#define CRSF_OPENTX_SLOW_BAUDRATE 115200 // Used for QX7 not supporting 400kbps
#define CRSF_NUM_CHANNELS 16
#define CRSF_CHANNEL_VALUE_MIN 172
#define CRSF_CHANNEL_VALUE_MID 992
#define CRSF_CHANNEL_VALUE_MAX 1811
#define CRSF_MAX_PACKET_LEN 64

#define CRSF_SYNC_BYTE 0xC8

#define RCframeLength sizeof(crsf_channels_t) // 22, length of the RC data packed bytes frame. 16 channels in 11 bits each.
//#define LinkStatisticsFrameLength 10 //
#define LinkStatisticsFrameLength sizeof(crsfPayloadLinkstatistics_s)
#define OpenTXsyncFrameLength 11                            //
#define BattSensorFrameLength sizeof(crsf_sensor_battery_t) // 8
#define VTXcontrolFrameLength 12                            //
#define LUArespLength 6

#define CRSF_PAYLOAD_SIZE_MAX 62
#define CRSF_FRAME_NOT_COUNTED_BYTES 2
#define CRSF_FRAME_SIZE(payload_size) ((payload_size) + 2) // See crsf_header_t.frame_size
#define CRSF_EXT_FRAME_SIZE(payload_size) (CRSF_FRAME_SIZE(payload_size) + 2)
#define CRSF_FRAME_SIZE_MAX (CRSF_PAYLOAD_SIZE_MAX + CRSF_FRAME_NOT_COUNTED_BYTES)

// Macros for big-endian (assume little endian host for now) etc
#define CRSF_DEC_U16(x) ((uint16_t)__builtin_bswap16(x))
#define CRSF_DEC_I16(x) ((int16_t)CRSF_DEC_U16(x))
#define CRSF_DEC_U24(x) (CRSF_DEC_U32((uint32_t)x << 8))
#define CRSF_DEC_U32(x) ((uint32_t)__builtin_bswap32(x))
#define CRSF_DEC_I32(x) ((int32_t)CRSF_DEC_U32(x))

//////////////////////////////////////////////////////////////

#define CRSF_MSP_REQ_PAYLOAD_SIZE 8
#define CRSF_MSP_RESP_PAYLOAD_SIZE 58
#define CRSF_MSP_MAX_PAYLOAD_SIZE (CRSF_MSP_REQ_PAYLOAD_SIZE > CRSF_MSP_RESP_PAYLOAD_SIZE ? CRSF_MSP_REQ_PAYLOAD_SIZE : CRSF_MSP_RESP_PAYLOAD_SIZE)

extern const unsigned char crc8tab[256];

#if 0
static const unsigned int VTXtable[6][8] =
    {{5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725},  /* Band A */
     {5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866},  /* Band B */
     {5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945},  /* Band E */
     {5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880},  /* Ariwave */
     {5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917},  /* Race */
     {5621, 5584, 5547, 5510, 5473, 5436, 5399, 5362}}; /* LO Race */
#endif

enum crsf_frame_type_e
{
    CRSF_FRAMETYPE_GPS = 0x02,
    CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
    CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
    CRSF_FRAMETYPE_OPENTX_SYNC = 0x10,
    CRSF_FRAMETYPE_RADIO_ID = 0x3A,
    CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,
    CRSF_FRAMETYPE_ATTITUDE = 0x1E,
    CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
    // Extended Header Frames, range: 0x28 to 0x96
    CRSF_FRAMETYPE_DEVICE_PING = 0x28,
    CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
    CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
    CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
    CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
    CRSF_FRAMETYPE_COMMAND = 0x32,
    // MSP commands
    CRSF_FRAMETYPE_MSP_REQ = 0x7A,   // response request using msp sequence as command
    CRSF_FRAMETYPE_MSP_RESP = 0x7B,  // reply with 58 byte chunked binary
    CRSF_FRAMETYPE_MSP_WRITE = 0x7C, // write with 8 byte chunked binary (OpenTX outbound telemetry buffer limit)
};

enum crsf_addr_e
{
    CRSF_ADDRESS_BROADCAST = 0x00,
    CRSF_ADDRESS_USB = 0x10,
    CRSF_ADDRESS_TBS_CORE_PNP_PRO = 0x80,
    CRSF_ADDRESS_RESERVED1 = 0x8A,
    CRSF_ADDRESS_CURRENT_SENSOR = 0xC0,
    CRSF_ADDRESS_GPS = 0xC2,
    CRSF_ADDRESS_TBS_BLACKBOX = 0xC4,
    CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,
    CRSF_ADDRESS_RESERVED2 = 0xCA,
    CRSF_ADDRESS_RACE_TAG = 0xCC,
    CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA,
    CRSF_ADDRESS_CRSF_RECEIVER = 0xEC,
    CRSF_ADDRESS_CRSF_TRANSMITTER = 0xEE,
};

typedef enum
{
    CRSF_UINT8 = 0,
    CRSF_INT8 = 1,
    CRSF_UINT16 = 2,
    CRSF_INT16 = 3,
    CRSF_UINT32 = 4,
    CRSF_INT32 = 5,
    CRSF_UINT64 = 6,
    CRSF_INT64 = 7,
    CRSF_FLOAT = 8,
    CRSF_TEXT_SELECTION = 9,
    CRSF_STRING = 10,
    CRSF_FOLDER = 11,
    CRSF_INFO = 12,
    CRSF_COMMAND = 13,
    CRSF_VTX = 15,
    CRSF_OUT_OF_RANGE = 127,
} crsf_value_type_e;

typedef struct crsf_header_s
{
    uint8_t device_addr; // from crsf_addr_e
    uint8_t frame_size;  // counts size after this byte, so it must be the payload size + 2 (type and crc)
    uint8_t type;        // from crsf_frame_type_e
} PACKED crsf_header_t;

// Used by extended header frames (type in range 0x28 to 0x96)
typedef struct crsf_ext_header_s
{
    // Common header fields, see crsf_header_t
    uint8_t device_addr;
    uint8_t frame_size;
    uint8_t type;
    // Extended fields
    uint8_t dest_addr;
    uint8_t orig_addr;
} PACKED crsf_ext_header_t;

typedef struct crsf_channels_s
{
    unsigned ch0 : 11;
    unsigned ch1 : 11;
    unsigned ch2 : 11;
    unsigned ch3 : 11;
    unsigned ch4 : 11;
    unsigned ch5 : 11;
    unsigned ch6 : 11;
    unsigned ch7 : 11;
    unsigned ch8 : 11;
    unsigned ch9 : 11;
    unsigned ch10 : 11;
    unsigned ch11 : 11;
    unsigned ch12 : 11;
    unsigned ch13 : 11;
    unsigned ch14 : 11;
    unsigned ch15 : 11;
} PACKED crsf_channels_t;
//typedef struct crsf_channels_s crsf_channels_t;

// Used by extended header frames (type in range 0x28 to 0x96)
typedef struct crsf_sensor_battery_s
{
    unsigned voltage : 16;  // mv * 100
    unsigned current : 16;  // ma * 100
    unsigned capacity : 24; // mah
    unsigned remaining : 8; // %
} PACKED crsf_sensor_battery_t;
//typedef struct crsf_sensor_battery_s crsf_sensor_battery_t;

/*
 * 0x14 Link statistics
 * Payload:
 *
 * uint8_t Uplink RSSI Ant. 1 ( dBm * -1 )
 * uint8_t Uplink RSSI Ant. 2 ( dBm * -1 )
 * uint8_t Uplink Package success rate / Link quality ( % )
 * int8_t Uplink SNR ( db )
 * uint8_t Diversity active antenna ( enum ant. 1 = 0, ant. 2 )
 * uint8_t RF Mode ( enum 4fps = 0 , 50fps, 150hz)
 * uint8_t Downlink TX Power ( enum 0mW = 0, 10mW, 25 mW, 100 mW, 500 mW, 1000 mW, 2000mW, 250mW )
 * uint8_t Downlink RSSI ( dBm * -1 )
 * uint8_t Downlink package success rate / Link quality ( % )
 * int8_t Downlink SNR ( db )
 *
 * Uplink is the connection from the ground to the UAV and downlink the opposite direction.
 * Uplink:   PILOT => UAV
 * Downlink: UAV   => PILOT
 */
typedef struct crsfPayloadLinkstatistics_s
{
    uint8_t uplink_RSSI_1;
    uint8_t uplink_RSSI_2;
    uint8_t uplink_Link_quality; // this goes to opentx rssi
    int8_t uplink_SNR;
    uint8_t active_antenna;
    uint8_t rf_Mode;
    uint8_t downlink_TX_Power;
    uint8_t downlink_RSSI;
    uint8_t downlink_Link_quality;
    int8_t downlink_SNR;
} crsfLinkStatistics_t;

/////inline and utility functions//////

//static uint16_t ICACHE_RAM_ATTR fmap(uint16_t x, float in_min, float in_max, float out_min, float out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; };
static uint16_t ICACHE_RAM_ATTR fmap(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; };

static inline uint16_t ICACHE_RAM_ATTR CRSF_to_US(uint16_t Val) { return round(fmap(Val, 172.0, 1811.0, 988.0, 2012.0)); };
static inline uint16_t ICACHE_RAM_ATTR UINT10_to_CRSF(uint16_t Val) { return round(fmap(Val, 0.0, 1024.0, 172.0, 1811.0)); };

static inline uint16_t ICACHE_RAM_ATTR SWITCH3b_to_CRSF(uint16_t Val) { return round(map(Val, 0, 7, 188, 1795)); };

// 2b switches use 0, 1 and 2 as values to represent low, middle and high
static inline uint16_t ICACHE_RAM_ATTR SWITCH2b_to_CRSF(uint16_t Val) { return round(map(Val, 0, 2, 188, 1795)); };

static inline uint8_t ICACHE_RAM_ATTR CRSF_to_BIT(uint16_t Val)
{
    if (Val > 1000)
        return 1;
    else
        return 0;
};
static inline uint16_t ICACHE_RAM_ATTR BIT_to_CRSF(uint8_t Val)
{
    if (Val)
        return 1795;
    else
        return 188;
};

static inline uint16_t ICACHE_RAM_ATTR CRSF_to_UINT10(uint16_t Val) { return round(fmap(Val, 172.0, 1811.0, 0.0, 1023.0)); };
//static inline uint16_t ICACHE_RAM_ATTR UINT_to_CRSF(uint16_t Val);

static inline uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t *data, int length)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

static inline uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t *data, int length)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

#if (CRSF_CMD_CRC)
extern const unsigned char crc8tabcmd[256];

static inline uint8_t ICACHE_RAM_ATTR CalcCRCcmd(uint8_t *data, int length)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        crc = crc8tabcmd[crc ^ *data++];
    }
    return crc;
}
#endif

class CRSF
{

public:
    CRSF() {}

    CRSF(HwSerial *dev)
    {
        _dev = dev;
    }
    CRSF(HwSerial &dev)
    {
        _dev = &dev;
    }

    void Begin();

    static VOLATILE uint16_t ChannelDataIn[16];
    static VOLATILE uint16_t ChannelDataInPrev[16]; // Contains the previous RC channel data
    static VOLATILE uint16_t ChannelDataOut[16];

// current and sent switch values
#define N_SWITCHES 8

    static uint8_t currentSwitches[N_SWITCHES];
    static uint8_t sentSwitches[N_SWITCHES];
    // which switch should be sent in the next rc packet
    static uint8_t nextSwitchIndex;

    static void (*RCdataCallback1)(); //function pointer for new RC data callback
    static void (*RCdataCallback2)(); //function pointer for new RC data callback

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvParameterUpdate)();

    static VOLATILE uint8_t ParameterUpdateData[2];

    /////Variables/////

    static volatile crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct
    static VOLATILE crsf_sensor_battery_s TLMbattSensor;

    /// UART Handling ///

    static bool CRSFstate;
    static bool CRSFstatePrev;
    static bool IsUARTslowBaudrate;

    static uint32_t UARTwdtNextCheck;
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;

#if (RX_MODULE)
    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void sendLinkStatisticsToFC();
    void RX_handleUartIn(void);

#elif (TX_MODULE)
    void ICACHE_RAM_ATTR sendLinkStatisticsToTX();
    void ICACHE_RAM_ATTR sendLinkBattSensorToTX();

    void ICACHE_RAM_ATTR sendLUAresponse(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4);

    void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel);

    uint8_t ICACHE_RAM_ATTR getNextSwitchIndex();

///// Variables for OpenTX Syncing //////////////////////////
#define OpenTXsyncPakcetInterval 100 // in ms

#if (FEATURE_OPENTX_SYNC)
    static VOLATILE uint32_t RCdataLastRecv;
    static VOLATILE uint32_t RequestedRCpacketInterval;
    static VOLATILE int32_t RequestedRCpacketAdvance;
    static VOLATILE uint32_t OpenTXsynNextSend;
    static VOLATILE int32_t OpenTXsyncOffset;
    void ICACHE_RAM_ATTR UpdateOpenTxSyncOffset(); // called from timer
    void ICACHE_RAM_ATTR sendSyncPacketToTX();
#endif
    /////////////////////////////////////////////////////////////

    void ICACHE_RAM_ATTR TX_handleUartIn(void);
#endif /* TX_MODULE */

    void process_input(void);

private:
    static HwSerial *_dev;

    static VOLATILE uint8_t SerialInPacketStart;
    static VOLATILE uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static VOLATILE uint8_t SerialInPacketPtr;                   // index where we are reading/writing
    static VOLATILE uint8_t SerialInBuffer[CRSF_MAX_PACKET_LEN]; // max 64 bytes for CRSF packet serial buffer

    static VOLATILE uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN + 1]; //index 0 hold the length of the datapacket

    static VOLATILE bool ignoreSerialData; //since we get a copy of the serial data use this flag to know when to ignore it
    static VOLATILE bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it

#if (TX_MODULE)
    void ICACHE_RAM_ATTR wdtUART();
    bool ICACHE_RAM_ATTR TX_ProcessPacket();
    void ICACHE_RAM_ATTR GetChannelDataIn();
    static void ICACHE_RAM_ATTR updateSwitchValues();
#elif (RX_MODULE)
    void ICACHE_RAM_ATTR CrsfFrameSendToFC(uint8_t *buff, uint8_t size);
    void RX_processPacket(void);
#endif

    static void inline nullCallback(void);

    void command_find_and_dispatch(void);
};

#endif
