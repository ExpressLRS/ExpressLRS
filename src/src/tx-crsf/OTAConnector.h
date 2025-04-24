#ifndef OTA_CONNECTOR_H
#define OTA_CONNECTOR_H

#include "CRSFEndPoint.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

class OTAConnector final : public CRSFConnector {
public:
    OTAConnector();

    void forwardMessage(crsf_ext_header_t *message) override;

    void AddMspMessage(uint8_t length, uint8_t *data);

    void pumpMSPSender();

    void ResetMspQueue();

private:
    void UnlockMspMessage();
    void GetMspMessage(uint8_t **data, uint8_t *len);

    static constexpr auto MSP_SERIAL_OUT_FIFO_SIZE = 256U;
    FIFO<MSP_SERIAL_OUT_FIFO_SIZE> MspWriteFIFO;
    uint8_t MspData[ELRS_MSP_BUFFER] = {};
    uint8_t MspDataLength = 0;
};

#endif //OTA_CONNECTOR_H
