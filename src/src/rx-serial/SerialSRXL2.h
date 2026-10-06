#pragma once
#if defined(TARGET_RX) || defined(SRXL2_ADAPTER_TEST)
#include "SerialIO.h"
#include "SRXL2.h"
#include "SRXL2Config.h"
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "esp_intr_alloc.h"
#endif
class SerialSRXL2 : public SerialIO
{
public:
    SerialSRXL2(Stream *output, Stream *input, int8_t txPin);
    ~SerialSRXL2() override;
    static void onRFReset();
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channels) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
#if defined(SRXL2_DIAGNOSTICS)
    struct Diagnostics
    {
        uint32_t frames = 0, txPackets = 0, rxBytes = 0;
        uint32_t driverInitUs = 0, firstTxUs = 0;
        volatile uint32_t txDone = 0;
        uint32_t txStartedUs = 0, txExpectedUs = 0;
        volatile uint32_t txDurationMaxUs = 0, txDelayMaxUs = 0;
        volatile uint32_t txDelayMinUs = 0xFFFFFFFFu, txDelayLongCount = 0;
        volatile uint32_t gpioEnableAfterTx = 0, gpioMatrixAfterTx = 0;
        uint32_t ch3 = 0xFFFF, ch3Min = 0xFFFF, ch3Max = 0;
        bool rfConnected = false, allowed = false, modelMatch = false, teamMatch = false, failsafe = false;
        uint8_t lastRfTx[16] = {}, lastRfTxLength = 0, rxTail[64] = {};
    };
    const Diagnostics &getDiagnostics() const { return diagnostics; }
    void event() override;
#endif
protected:
    void processBytes(uint8_t *bytes, uint16_t size) override;
private:
    SRXL2::Link link;
    int8_t pin;
    Stream *inputPort;
    bool transmitting = false;
    bool txReady = true;
    volatile bool txComplete = false;
    volatile uint32_t txEnded = 0;
#if defined(CONFIG_IDF_TARGET_ESP32)
    intr_handle_t txInterrupt = nullptr;
#endif
    bool skipNextFrame = true;
    uint32_t lastPublished = 0;
    uint8_t nextSensor = 0;
    uint32_t generation = 0, lastHardwareReceive = 0;
#if defined(SRXL2_DIAGNOSTICS)
    Diagnostics diagnostics;
    bool diagnosticPublished = false;
#endif
    bool controlAllowed() const;
    bool synchronizeGeneration();
    void completeTransmission(uint32_t now);
    static void onTxDone(void *argument);
    void publishTelemetry(uint32_t now);
};
#endif
