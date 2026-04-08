#pragma once

#include "CRSFEndpoint.h"
#include "msptypes.h"

class RxTxEndpoint : public CRSFEndpoint {
public:
    explicit RxTxEndpoint(const crsf_addr_e device_id)
        : CRSFEndpoint(device_id) {}
    ~RxTxEndpoint() override = default;

protected:
    bool handleRxTxMessage(const crsf_header_t *message);

    virtual void handleMspGetRxTxConfig(crsf_ext_header_t *extMessage);
    virtual void handleMspSetRxTxConfig(crsf_ext_header_t *extMessage);
};
