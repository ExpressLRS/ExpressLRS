#ifndef TX_OTA_CONNECTOR_H
#define TX_OTA_CONNECTOR_H

#include "CRSFEndpoint.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

class TXOTAConnector final : public CRSFConnector {
public:
    TXOTAConnector();

    void forwardMessage(crsf_header_t *message) override;

    void AddMspMessage(uint8_t length, uint8_t *data);

    void ResetMspQueue();

    void pumpMspSender();

private:
    void UnlockMspMessage();
    void GetMspMessage(uint8_t **data, uint8_t *len);

    static constexpr auto MSP_SERIAL_OUT_FIFO_SIZE = 256U;
    FIFO<MSP_SERIAL_OUT_FIFO_SIZE> MspWriteFIFO;
    uint8_t MspData[ELRS_MSP_BUFFER] = {};
    uint8_t MspDataLength = 0;
};

#endif //TX_OTA_CONNECTOR_H
