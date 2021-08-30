#ifndef H_CRSF
#define H_CRSF

#include "targets.h"
#include "crsf_protocol.h"
#ifndef TARGET_NATIVE
#include "HardwareSerial.h"
#endif
#include "msp.h"
#include "msptypes.h"
#include "LowPassFilter.h"
#include "../CRC/crc.h"
#include "telemetry_protocol.h"

#ifdef PLATFORM_ESP32
#include "esp32-hal-uart.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#endif

class CRSF
{

public:
    #if CRSF_RX_MODULE

    CRSF(Stream *dev) : _dev(dev)
    {
    }

    CRSF(Stream &dev) : _dev(&dev) {}

    #endif

    static HardwareSerial Port;

    static volatile uint16_t ChannelDataIn[16];
    static volatile uint16_t ChannelDataOut[16];

    // The model ID as received from the Transmitter
    static uint8_t modelId;

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvModelUpdate)();
    static void (*RecvParameterUpdate)();
    static void (*RCdataCallback)();

    static volatile uint8_t ParameterUpdateData[3];
    static volatile bool elrsLUAmode;

    /////Variables/////

    static volatile crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct
    static volatile crsf_sensor_battery_s TLMbattSensor;

    /// UART Handling ///
    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult; // need to latch the results

    static bool hasEverConnected;

    static void Begin(); //setup timers etc
    static void End(); //stop timers etc

    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendMSPFrameToFC(uint8_t* data);
    void sendLinkStatisticsToFC();
    void ICACHE_RAM_ATTR sendLinkStatisticsToTX();
    void ICACHE_RAM_ATTR sendTelemetryToTX(uint8_t *data);

    uint8_t sendCRSFparam(crsf_frame_type_e frame,uint8_t fieldchunk, crsf_value_type_e dataType, const void * luaData, uint8_t wholePacketSize);
    static void packetQueueExtended(uint8_t type, void *data, uint8_t len);

    static void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel);

    uint8_t getModelID() const { return modelId; }

    ///// Variables for OpenTX Syncing //////////////////////////
    #define OpenTXsyncPacketInterval 200 // in ms
    static void ICACHE_RAM_ATTR setSyncParams(uint32_t PacketInterval);
    static void ICACHE_RAM_ATTR JustSentRFpacket();
    static void ICACHE_RAM_ATTR sendSyncPacketToTX();
    static void disableOpentxSync();
    static void enableOpentxSync();

    /////////////////////////////////////////////////////////////

    static void ICACHE_RAM_ATTR GetChannelDataIn();
    static uint32_t ICACHE_RAM_ATTR GetRCdataLastRecv();
    static void ICACHE_RAM_ATTR updateSwitchValues();

    static void inline nullCallback(void);

    static void handleUARTin();
    bool RXhandleUARTout();
#if CRSF_TX_MODULE
    static uint8_t* GetMspMessage();
    static void UnlockMspMessage();
    static void AddMspMessage(const uint8_t length, volatile uint8_t* data);
    static void AddMspMessage(mspPacket_t* packet);
    static void ResetMspQueue();
    static volatile uint32_t OpenTXsyncLastSent;

    uint8_t setLuaHiddenFlag(uint8_t id, bool value);

#endif
private:
    Stream *_dev;

    static volatile uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketPtr;                   // index where we are reading/writing

    static volatile inBuffer_U inBuffer;

    static volatile bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it

#if CRSF_TX_MODULE
    /// OpenTX mixer sync ///
    static uint32_t RequestedRCpacketInterval;
    static volatile uint32_t RCdataLastRecv;
    static volatile int32_t OpenTXsyncOffset;
    static uint32_t OpenTXsyncOffsetSafeMargin;
    static bool OpentxSyncActive;
    static uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN];
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
    static uint32_t SyncWaitPeriodCounter;
#endif

    /// UART Handling ///
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;
    static uint32_t UARTwdtLastChecked;
    static uint32_t UARTcurrentBaud;
    static bool CRSFstate;
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;
#ifdef PLATFORM_ESP32
    static void ESP32uartTask(void *pvParameters);
    static void ESP32syncPacketTask(void *pvParameters);
#endif

    static void duplex_set_RX();
    static void duplex_set_TX();
    static bool ProcessPacket();
    static void handleUARTout();
    static bool UARTwdt();
    
    static uint32_t luaHiddenFlags;

    void getLuaTextSelectionStructToArray(const void * luaStruct, uint8_t *outarray);
    void getLuaCommandStructToArray(const void * luaStruct, uint8_t *outarray);
    void getLuaUint8StructToArray(const void * luaStruct, uint8_t *outarray);
    void getLuaUint16StructToArray(const void * luaStruct, uint8_t *outarray);
    void getLuaStringStructToArray(const void * luaStruct, uint8_t *outarray);
    void getLuaFolderStructToArray(const void * luaStruct, uint8_t *outarray);
      /** we dont need these yet for OUR LUA, and it is not defined yet
     void getLuaUint8StructToArray(const void * luaStruct, uint8_t *outarray);
     void getLuaint8StructToArray(const void * luaStruct, uint8_t *outarray);
     void getLuaUint16StructToArray(const void * luaStruct, uint8_t *outarray);
     void getLuaint16StructToArray(const void * luaStruct, uint8_t *outarray);
     void getLuaFloatStructToArray(const void * luaStruct, uint8_t *outarray);
*/ 

#endif

    static void flush_port_input(void);


};

#endif
