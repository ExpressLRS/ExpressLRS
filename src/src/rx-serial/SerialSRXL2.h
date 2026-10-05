#pragma once
#if defined(TARGET_RX) || defined(SRXL2_ADAPTER_TEST)
#include "SerialIO.h"
#include "SRXL2.h"
class SerialSRXL2 : public SerialIO
{
public:
    SerialSRXL2(Stream *output, Stream *input, int8_t txPin);
    ~SerialSRXL2() override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channels) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
protected:
    void processBytes(uint8_t *bytes, uint16_t size) override;
private:
    SRXL2::Link link;
    int8_t pin;
    bool transmitting = false;
    uint32_t lastPublished = 0;
    uint8_t nextSensor = 0;
    bool controlAllowed() const;
    void completeTransmission(uint32_t now);
    void publishTelemetry(uint32_t now);
};
#endif
