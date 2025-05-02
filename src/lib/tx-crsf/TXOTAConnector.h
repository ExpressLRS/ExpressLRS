#ifndef TX_OTA_CONNECTOR_H
#define TX_OTA_CONNECTOR_H

#include "CRSFEndpoint.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

class TXOTAConnector final : public CRSFConnector {
public:
    TXOTAConnector();

    void forwardMessage(const crsf_header_t *message) override;

    void resetOutputQueue();

    void pumpSender();

private:
    void unlockMessage();

    static constexpr auto MSP_SERIAL_OUT_FIFO_SIZE = 256U;
    FIFO<MSP_SERIAL_OUT_FIFO_SIZE> outputQueue;
    uint8_t currentTransmissionBuffer[ELRS_MSP_BUFFER] = {};
    uint8_t currentTransmissionLength = 0;
};

#endif //TX_OTA_CONNECTOR_H
