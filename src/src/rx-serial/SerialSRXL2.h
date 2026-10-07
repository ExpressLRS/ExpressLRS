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
    SerialSRXL2(Stream *output, Stream *input, int8_t txPin, uint8_t serialPort = 0);
    ~SerialSRXL2() override;
    static void onRFReset();
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channels) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
protected:
    void processBytes(uint8_t *bytes, uint16_t size) override;
private:
    SRXL2::Link link;
    int8_t pin;
    uint8_t port;
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
    bool controlAllowed() const;
    void synchronizeGeneration();
    void completeTransmission(uint32_t now);
    static void onTxDone(void *argument);
    void publishTelemetry(uint32_t now);
};
#endif
