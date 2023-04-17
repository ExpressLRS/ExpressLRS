#ifndef H_CRSF
#define H_CRSF

#include "targets.h"
#include "crsf_protocol.h"
#ifndef TARGET_NATIVE
#include "HardwareSerial.h"
#endif
#include "common.h"
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
    /////Variables/////

    #ifdef CRSF_TX_MODULE
    static HardwareSerial Port;
    static Stream *PortSecondary; // A second UART used to mirror telemetry out on the TX, not read from

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvModelUpdate)();
    static void (*RecvParameterUpdate)(uint8_t type, uint8_t index, uint8_t arg);
    static void (*RCdataCallback)();

    // The model ID as received from the Transmitter
    static uint8_t modelId;
    static bool ForwardDevicePings; // true if device pings should be forwarded OTA
    static bool elrsLUAmode;

    /// UART Handling ///
    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult; // need to latch the results
    #endif

    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct

    static void Begin(); //setup timers etc
    static void End(); //stop timers etc

    static void GetDeviceInformation(uint8_t *frame, uint8_t fieldCount);
    static void SetHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t destAddr);
    static void SetExtendedHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t senderAddr, uint8_t destAddr);
    static uint32_t VersionStrToU32(const char *verStr);

    #ifdef CRSF_TX_MODULE
    static bool IsArmed() { return CRSF_to_BIT(ChannelData[4]); } // AUX1
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
    static void AddMspMessage(const uint8_t length, uint8_t* data);
    static void AddMspMessage(mspPacket_t* packet);
    static void ResetMspQueue();
    static uint32_t OpenTXsyncLastSent;
    static uint8_t GetMaxPacketBytes() { return maxPacketBytes; }
    static uint32_t GetCurrentBaudRate() { return UARTrequestedBaud; }

    static uint32_t ICACHE_RAM_ATTR GetRCdataLastRecv();
    static void ICACHE_RAM_ATTR RcPacketToChannelsData();
    #endif

    /////////////////////////////////////////////////////////////
    static bool CRSFstate;

private:
    static inBuffer_U inBuffer;

#if CRSF_TX_MODULE
    /// OpenTX mixer sync ///
    static uint32_t RequestedRCpacketInterval;
    static volatile uint32_t RCdataLastRecv;
    static volatile uint32_t dataLastRecv;
    static volatile int32_t OpenTXsyncOffset;
    static volatile int32_t OpenTXsyncWindow;
    static volatile int32_t OpenTXsyncWindowSize;
    static uint32_t OpenTXsyncOffsetSafeMargin;
    static bool OpentxSyncActive;
    static uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN];

    /// UART Handling ///
    static uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static uint8_t SerialInPacketPtr;                   // index where we are reading/writing
    static bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;
    static uint32_t UARTwdtLastChecked;
    static uint8_t maxPacketBytes;
    static uint8_t maxPeriodBytes;
    static uint32_t TxToHandsetBauds[7];
    static uint8_t UARTcurrentBaudIdx;
    static uint32_t UARTrequestedBaud;
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;
    #if defined(PLATFORM_ESP32)
    static bool UARTinverted;
    #endif

    static void ICACHE_RAM_ATTR adjustMaxPacketSize();
    static void duplex_set_RX();
    static void duplex_set_TX();
    static bool ProcessPacket();
    static void handleUARTout();
    static bool UARTwdt();
    static uint32_t autobaud();
    static void flush_port_input(void);
#endif
};

extern GENERIC_CRC8 crsf_crc;

#endif
