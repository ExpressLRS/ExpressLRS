#pragma once

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

#if CRSF_TX_MODULE

class CRSF_TXModule : public TXModule
{

public:
    CRSF_TXModule() : TXModule()
    {}

    static void (*disconnected)();
    static void (*connected)();
    static void (*RecvParameterUpdate)();

    static volatile uint8_t ParameterUpdateData[2];

    /// UART Handling ///
    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult; // need to latch the results

    void begin(TransportLayer* dev) override; //setup timers etc

    void ICACHE_RAM_ATTR sendLinkStatisticsToTX(Channels* chan);
    void ICACHE_RAM_ATTR sendTelemetryToTX(uint8_t *data);
    void ICACHE_RAM_ATTR sendSyncPacketToTX() override;

    void sendLUAresponse(uint8_t val[], uint8_t len);

    static void ICACHE_RAM_ATTR GetChannelDataIn(Channels* chan);
    void consumeInputByte(uint8_t in, Channels* chan) override;

    bool UARTwdt();

    static uint8_t* GetMspMessage();
    static void UnlockMspMessage();
    static void AddMspMessage(const uint8_t length, volatile uint8_t* data);
    static void AddMspMessage(mspPacket_t* packet);
    static void ResetMspQueue();

private:
    
    // since we get a copy of the serial data use this flag to know when to ignore it
    static volatile bool CRSFframeActive;

    static volatile inBuffer_U inBuffer;

    // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketLen;
    // index where we are reading/writing
    static volatile uint8_t SerialInPacketPtr;

    /// UART Handling ///
    static uint32_t GoodPktsCount;
    static uint32_t BadPktsCount;
    static bool CRSFstate;
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;
    static volatile uint8_t MspRequestsInTransit;
    static uint32_t LastMspRequestSent;

    bool ProcessPacket(Channels* chan);
    void flushTxBuffers() override;

};

extern CRSF_TXModule crsfTx;

#endif

#if CRSF_RX_MODULE        

class CRSF_RXModule
{
public:
    CRSF_RXModule() {}

    void begin(TransportLayer* dev);

    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendMSPFrameToFC(uint8_t* data);
    void sendLinkStatisticsToFC();

    /////////////////////////////////////////////////////////////
private:
    TransportLayer* _dev = nullptr;
};

extern CRSF_RXModule crsfRx;

#endif


// Serial settings:
//  TODO: this needs to go away
extern uint32_t UARTwdtLastChecked;
extern uint32_t UARTcurrentBaud;

// for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000
