#ifndef H_CRSF
#define H_CRSF

#include <Arduino.h>
#include <crsf_protocol.h>
#include "HardwareSerial.h"
#include "msp.h"
#include "msptypes.h"
#include "../../src/targets.h"
#include "../../src/LowPassFilter.h"

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
    #if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX) || defined(UNIT_TEST)

    CRSF(Stream *dev) : _dev(dev)
    {
    }

    CRSF(Stream &dev) : _dev(&dev) {}

    #endif

    static HardwareSerial Port;

    static volatile uint16_t ChannelDataIn[16];
    static volatile uint16_t ChannelDataInPrev[16]; // Contains the previous RC channel data RX side only
    static volatile uint16_t ChannelDataOut[16];

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

    static volatile uint8_t ParameterUpdateData[2];

    static uint8_t CSFR_TXpin_Module;
    static uint8_t CSFR_RXpin_Module;

    static uint8_t CSFR_TXpin_Recv;
    static uint8_t CSFR_RXpin_Recv;

    /////Variables/////

    static volatile crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct
    static volatile crsf_sensor_battery_s TLMbattSensor;

    static void Begin(); //setup timers etc
    static void End(); //stop timers etc

    /// UART Handling ///

    static bool CRSFstate;
    static bool CRSFstatePrev;
    static uint32_t UARTcurrentBaud;
    static uint32_t UARTrequestedBaud;

    static uint32_t lastUARTpktTime;
    static uint32_t UARTwdtLastChecked;
    static uint32_t UARTwdtInterval;

    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;

#ifdef PLATFORM_ESP32
    static void ICACHE_RAM_ATTR ESP32uartTask(void *pvParameters);
    static void ICACHE_RAM_ATTR UARTwdt(void *pvParametersxHandleSerialOutFIFO);
    static void ICACHE_RAM_ATTR duplex_set_RX();
    static void ICACHE_RAM_ATTR duplex_set_TX();
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)
    static void ICACHE_RAM_ATTR STM32initUART();
    static void ICACHE_RAM_ATTR UARTwdt();
    static void ICACHE_RAM_ATTR STM32handleUARTin();
    static void ICACHE_RAM_ATTR STM32handleUARTout();
#endif

    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendMSPFrameToFC(mspPacket_t* packet);
    void sendLinkStatisticsToFC();
    void ICACHE_RAM_ATTR sendLinkStatisticsToTX();
    void ICACHE_RAM_ATTR sendLinkBattSensorToTX();

    void sendLUAresponse(uint8_t val[]);

    static void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel);

    uint8_t ICACHE_RAM_ATTR getNextSwitchIndex();
    void ICACHE_RAM_ATTR setSentSwitch(uint8_t index, uint8_t value);

///// Variables for OpenTX Syncing //////////////////////////
    #define OpenTXsyncPacketInterval 200 // in ms
    static volatile uint32_t OpenTXsyncLastSent;
    static uint32_t RequestedRCpacketInterval;
    static volatile uint32_t RCdataLastRecv;
    static volatile int32_t OpenTXsyncOffset;
    static uint32_t OpenTXsyncOffsetSafeMargin;
    static int32_t OpenTXsyncOffetFLTR;
    static uint32_t SyncWaitPeriodCounter;
    static void ICACHE_RAM_ATTR setSyncParams(uint32_t PacketInterval);
    static void ICACHE_RAM_ATTR JustSentRFpacket();
    static void ICACHE_RAM_ATTR sendSyncPacketToTX(void *pvParameters);
    static void ICACHE_RAM_ATTR sendSyncPacketToTX();
    /////////////////////////////////////////////////////////////

    static void ICACHE_RAM_ATTR ESP32ProcessPacket();
    static bool ICACHE_RAM_ATTR STM32ProcessPacket();
    static void ICACHE_RAM_ATTR GetChannelDataIn();
    static void ICACHE_RAM_ATTR updateSwitchValues();
    bool RXhandleUARTout();

    static void inline nullCallback(void);


private:
    Stream *_dev;

    static volatile uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketPtr;                   // index where we are reading/writing

    static volatile inBuffer_U inBuffer;
    static volatile uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN + 1]; //index 0 hold the length of the datapacket

    static volatile bool ignoreSerialData; //since we get a copy of the serial data use this flag to know when to ignore it
    static volatile bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it
};

#endif
