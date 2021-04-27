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
#include "TXModule.h"

#ifdef PLATFORM_ESP32
#include "esp32-hal-uart.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#endif


class CRSF
#if CRSF_TX_MODULE
    : public TXModule
#endif
{

public:
    CRSF()
#if CRSF_TX_MODULE
        : TXModule()
#endif
    {}


    static volatile uint16_t ChannelDataIn[16];
    static volatile uint16_t ChannelDataOut[16];

    // current and sent switch values
    #define N_SWITCHES 8

    static uint8_t currentSwitches[N_SWITCHES];
    static uint8_t sentSwitches[N_SWITCHES];
    // which switch should be sent in the next rc packet
    static uint8_t nextSwitchIndex;

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvParameterUpdate)();

    static volatile uint8_t ParameterUpdateData[2];

    /////Variables/////

    static volatile crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct
    static volatile crsf_sensor_battery_s TLMbattSensor;

    /// UART Handling ///
    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult; // need to latch the results

    void begin(Stream* dev); //setup timers etc
    void end(); //stop timers etc

    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendMSPFrameToFC(uint8_t* data);
    void sendLinkStatisticsToFC();
    void ICACHE_RAM_ATTR sendLinkStatisticsToTX();
    void ICACHE_RAM_ATTR sendTelemetryToTX(uint8_t *data);

    void sendLUAresponse(uint8_t val[], uint8_t len);

    static void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel);

    uint8_t ICACHE_RAM_ATTR getNextSwitchIndex();
    void ICACHE_RAM_ATTR setSentSwitch(uint8_t index, uint8_t value);

///// Variables for OpenTX Syncing //////////////////////////
#if CRSF_TX_MODULE
    void ICACHE_RAM_ATTR sendSyncPacketToTX() override;
#endif

    /////////////////////////////////////////////////////////////

    static void ICACHE_RAM_ATTR GetChannelDataIn();
    static void ICACHE_RAM_ATTR updateSwitchValues();

    static void inline nullCallback(void);

    void handleUARTin();
    bool RXhandleUARTout();

    void flush_port_input(void);

#if CRSF_TX_MODULE
    static void duplex_set_RX();
    static void duplex_set_TX();

    static uint8_t* GetMspMessage();
    static void UnlockMspMessage();
    static void AddMspMessage(const uint8_t length, volatile uint8_t* data);
    static void AddMspMessage(mspPacket_t* packet);
    static void ResetMspQueue();
#endif
private:
    Stream *_dev = nullptr;

    static volatile uint8_t SerialInPacketLen;                   // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketPtr;                   // index where we are reading/writing

    static volatile inBuffer_U inBuffer;
    static volatile uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN + 1]; //index 0 hold the length of the datapacket

    static volatile bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it

#if CRSF_TX_MODULE

    /// UART Handling ///
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;
    static bool CRSFstate;
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;
    static volatile uint8_t MspRequestsInTransit;
    static uint32_t LastMspRequestSent;

    bool ProcessPacket();
    void flushTxBuffers() override;
    bool UARTwdt();
#endif
};

extern CRSF crsf;

// Serial settings:
//  TODO: this needs to go away
extern uint32_t UARTwdtLastChecked;
extern uint32_t UARTcurrentBaud;

// for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000

#endif
