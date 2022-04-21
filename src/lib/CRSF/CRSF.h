#ifndef H_CRSF
#define H_CRSF

#include "targets.h"
#include "crsf_protocol.h"
#if defined(PLATFORM_ESP8266) && defined(CRSF_RX_MODULE) && defined(USE_MSP_WIFI)
#include "crsf2msp.h"
#include "msp2crsf.h"
#endif
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

    #if defined(PLATFORM_ESP8266) && defined(USE_MSP_WIFI)
    static CROSSFIRE2MSP crsf2msp;
    static MSP2CROSSFIRE msp2crsf;
    #endif
    #endif


    static HardwareSerial Port;
    static Stream *PortSecondary; // A second UART used to mirror telemetry out on the TX, not read from

    static volatile uint16_t ChannelDataIn[16];

    /////Variables/////


    static volatile uint8_t ParameterUpdateData[3];

    #ifdef CRSF_TX_MODULE
    static void inline nullCallback(void);

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvModelUpdate)();
    static void (*RecvParameterUpdate)();
    static void (*RCdataCallback)();

    // The model ID as received from the Transmitter
    static uint8_t modelId;
    static bool ForwardDevicePings; // true if device pings should be forwarded OTA
    static volatile bool elrsLUAmode;

    /// UART Handling ///
    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult; // need to latch the results
    #endif

    #ifdef CRSF_RX_MODULE
    static crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static uint16_t GetChannelOutput(uint8_t ch);
    static bool lastArmingState;
    #endif

    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct

    static void Begin(); //setup timers etc
    static void End(); //stop timers etc

    static void GetDeviceInformation(uint8_t *frame, uint8_t fieldCount);
    static void SetHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t destAddr);
    static void SetExtendedHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t senderAddr, uint8_t destAddr);

    #ifdef CRSF_TX_MODULE
    static void ICACHE_RAM_ATTR sendLinkStatisticsToTX();
    static void ICACHE_RAM_ATTR sendTelemetryToTX(uint8_t *data);

    static void packetQueueExtended(uint8_t type, void *data, uint8_t len);

    static void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel);

    ///// Variables for OpenTX Syncing //////////////////////////
    #define OpenTXsyncPacketInterval 200 // in ms
    static void ICACHE_RAM_ATTR setSyncParams(uint32_t PacketInterval);
    static void ICACHE_RAM_ATTR JustSentRFpacket();
    static void ICACHE_RAM_ATTR sendSyncPacketToTX();
    static void disableOpentxSync();
    static void enableOpentxSync();

    static void handleUARTin();

    static uint8_t getModelID() { return modelId; }

    static void GetMspMessage(uint8_t **data, uint8_t *len);
    static void UnlockMspMessage();
    static void AddMspMessage(const uint8_t length, volatile uint8_t* data);
    static void AddMspMessage(mspPacket_t* packet);
    static void ResetMspQueue();
    static volatile uint32_t OpenTXsyncLastSent;
    static uint8_t GetMaxPacketBytes() { return maxPacketBytes; }
    static uint32_t GetCurrentBaudRate() { return TxToHandsetBauds[UARTcurrentBaudIdx]; }

    static uint32_t ICACHE_RAM_ATTR GetRCdataLastRecv();
    static void ICACHE_RAM_ATTR updateSwitchValues();
    static void ICACHE_RAM_ATTR GetChannelDataIn();
    #endif

    #ifdef CRSF_RX_MODULE
    bool RXhandleUARTout();
    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendMSPFrameToFC(uint8_t* data);
    void sendLinkStatisticsToFC();
    #endif

    /////////////////////////////////////////////////////////////
    static bool CRSFstate;

private:
    Stream *_dev;

    static inBuffer_U inBuffer;

#if CRSF_TX_MODULE
    /// OpenTX mixer sync ///
    static uint32_t RequestedRCpacketInterval;
    static volatile uint32_t RCdataLastRecv;
    static volatile int32_t OpenTXsyncOffset;
    static uint32_t OpenTXsyncOffsetSafeMargin;
    static bool OpentxSyncActive;
    static uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN];

    /// UART Handling ///
    static volatile uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketPtr;                   // index where we are reading/writing
    static volatile bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;
    static uint32_t UARTwdtLastChecked;
    static uint8_t maxPacketBytes;
    static uint8_t maxPeriodBytes;
    static uint32_t TxToHandsetBauds[6];
    static uint8_t UARTcurrentBaudIdx;
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;

    static void ICACHE_RAM_ATTR adjustMaxPacketSize();
    static void duplex_set_RX();
    static void duplex_set_TX();
    static bool ProcessPacket();
    static void handleUARTout();
    static bool UARTwdt();
#endif

    static void flush_port_input(void);
};

extern GENERIC_CRC8 crsf_crc;

#endif
