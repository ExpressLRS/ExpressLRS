#pragma once

#include <cstdint>
#include "crsf_common.h"
#include "crsf_protocol.h"
#include "FIFO.h"

class CROSSFIRE
{
private:
    HardwareSerial *UARTport;

    FIFO UARTinFIFO;
    FIFO UARToutFIFO;

    uint16_t RCchannelDataRaw[16] = {0};

    /// Data Callbacks ///
    void (*recvRCdata)();  // called when new MSP data is recieved
    void (*recvMSPdata)(); // called when there is new RC data

    /// Mixer Sync (aka crsfshot) //////////////////////////////////////////////
    uint32_t syncReqPktInterval; // 1/(packet rate) in 10x microseconds
    bool syncActive;
    void syncPushPktToTX();

    /// UART Handling /////////////////////////////////////////////////////////
    void (*UARTconnected)();    // called when CRSF stream is regained
    void (*UARTdisconnected)(); // called when CRSF stream is lost
    bool UARTisConnected;
    uint8_t UARTbufWriteIdx;
    uint8_t UARTbufLen;
    uint8_t UARTinBuffer[CRSF_MAX_PACKET_LEN]; // 64 bytes max
    const uint32_t CRSFbaudrates[6] = {400000, 115200, 5250000, 3750000, 1870000, 921600};
    uint8_t UARTcurrentBaudIdx;

    const uint32_t UARTwdtInterval = 1000;
    uint32_t UARTgoodPktsCount;
    uint32_t UARTbadPktsCount;
    uint32_t UARTwatchdogLastReset;
    void UARTcallbackConnected(void (*callback)(void));
    void UARTcallbackDisconnected(void (*callback)(void));
    void UARTinit();
    void UARTout();
    void UARTin();
    void UARTwatchdog();
    uint32_t UARTgetGoodPktsCount();
    uint32_t UARTgetBadPktsCount();
    void UARTduplexSetTX();
    void UARTduplexSetRX();
    void UARTflush();

public:
    CROSSFIRE(HardwareSerial *obj);
    ~CROSSFIRE();

    void begin();
    void end();

    /// Mixer Sync (aka crsfshot) //////////////////////////////////////////////
    void syncSetParams(uint32_t interval);
    void syncEvent(uint32_t timestamp);
    bool syncActive();

    void setRCdataCallback(void (*callback)(void));
    void setMSPdataCallback(void (*callback)(void));

    void processFrame();
    void processRCframe();
    void processMSPframe();
    void processTLMframe();
};

CROSSFIRE::CROSSFIRE(HardwareSerial *obj){};
CROSSFIRE::~CROSSFIRE(){};
