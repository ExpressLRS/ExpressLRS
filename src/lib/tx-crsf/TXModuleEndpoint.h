#ifndef TX_MODULE_CRSF_H
#define TX_MODULE_CRSF_H
#include "CRSFEndpoint.h"
#include "msp.h"

class TXModuleEndpoint final : public CRSFEndpoint {
public:
    TXModuleEndpoint();
    ~TXModuleEndpoint() override = default;
    void begin();

    bool handleRaw(const crsf_header_t *message) override;
    void handleMessage(const crsf_header_t *message) override;
    void RcPacketToChannelsData(const crsf_header_t *message);

    void devicePingCalled() override;
    void luadevUpdateFolderNames();
    void registerParameters() override;
    void updateParameters() override;

    uint8_t modelId; // The model ID as received from the Transmitter
    bool armCmd = false; // Arm command from handset either via ch5 or arm message
    bool lastArmCmd = false;
};

#endif //TX_MODULE_CRSF_H
