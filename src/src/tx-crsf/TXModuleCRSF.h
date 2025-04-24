#ifndef TX_MODULE_CRSF_H
#define TX_MODULE_CRSF_H
#include "CRSFEndPoint.h"
#include "msp.h"

class TXModuleCRSF final : public CRSFEndPoint {
public:
    TXModuleCRSF();
    void begin();

    bool handleMessage(crsf_ext_header_t *message) override;
    void RcPacketToChannelsData(crsf_ext_header_t *message);

    void (*RecvModelUpdate)() = nullptr; // called when model id changes, ie command from Radio
    void (*OnBindingCommand)() = nullptr; // Called when a binding command is received
    void (*RecvParameterUpdate)(crsf_addr_e origin, uint8_t type, uint8_t index, uint8_t arg) = nullptr; // called when recv parameter update req, ie from LUA

    uint8_t modelId; // The model ID as received from the Transmitter
    bool armCmd = false; // Arm command from handset either via ch5 or arm message
    bool lastArmCmd = false;
};

#endif //TX_MODULE_CRSF_H
