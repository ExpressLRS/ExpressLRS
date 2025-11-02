#ifndef H_CRSF_CONTROLLER
#define H_CRSF_CONTROLLER

#ifdef TARGET_TX

#include "handset.h"
#include "crsf_protocol.h"
#ifndef TARGET_NATIVE
#include "HardwareSerial.h"
#endif
#include "CRSFConnector.h"
#include "common.h"

#ifdef PLATFORM_ESP32
#include "driver/uart.h"
#endif

class CRSFHandset final : public Handset, public CRSFConnector
{

public:
    /////Variables/////
    static HardwareSerial Port;

    static uint32_t GoodPktsCountResult; // need to latch the results
    static uint32_t BadPktsCountResult;  // need to latch the results

    void Begin() override;
    void End() override;

    void forwardMessage(const crsf_header_t *message) override;

    void handleInput() override;
    void handleOutput(uint32_t receivedBytes);

    void setPacketInterval(int32_t PacketInterval) override;
    void JustSentRFpacket() override;

    uint8_t GetMaxPacketBytes() const override { return maxPacketBytes; }
    int getMinPacketInterval() const override;

private:
    uint8_t inBuffer[CRSF_MAX_PACKET_LEN] = {};

    /// OpenTX mixer sync ///
    volatile uint32_t dataLastRecv = 0;
    volatile int32_t OpenTXsyncOffset = 0;
    volatile int32_t OpenTXsyncWindow = 0;
    volatile int32_t OpenTXsyncWindowSize = 0;
    uint32_t OpenTXsyncLastSent = 0;

    /// UART Handling ///
    uint8_t SerialInPacketPtr = 0; // index where we are reading/writing
    static bool halfDuplex;
    bool transmitting = false;
    uint32_t GoodPktsCount = 0;
    uint32_t BadPktsCount = 0;
    uint32_t UARTwdtLastChecked = 0;
    uint8_t maxPacketBytes = CRSF_MAX_PACKET_LEN;
    uint8_t maxPeriodBytes = CRSF_MAX_PACKET_LEN;

    static uint8_t UARTcurrentBaudIdx;
    static uint32_t UARTrequestedBaud;

    static Stream *PortSecondary; // A second UART used to mirror telemetry out on the TX, not read from

#if defined(PLATFORM_ESP32)
    bool UARTinverted = false;
#endif

    void sendSyncPacketToTX();
    void adjustMaxPacketSize();
    void duplex_set_RX() const;
    void duplex_set_TX() const;
    void alignBufferToSync(uint8_t startIdx);
    bool ProcessPacket();
    bool UARTwdt();
    uint32_t autobaud();
    void flush_port_input();
};

#endif
#endif
