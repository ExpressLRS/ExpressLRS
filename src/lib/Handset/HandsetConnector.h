#ifndef HANDSET_CONNECTOR_H
#define HANDSET_CONNECTOR_H
#include "CRSFConnector.h"

class HandsetConnector final : public CRSFConnector {
public:
    HandsetConnector();
    void forwardMessage(crsf_ext_header_t *message) override;
    uint32_t VersionStrToU32(const char *verStr);
    void GetDeviceInformation(uint8_t *frame, uint8_t fieldCount);
};

#endif //HANDSET_CONNECTOR_H
