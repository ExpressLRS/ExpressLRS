#ifndef RX_ENDPOINT_H
#define RX_ENDPOINT_H
#include "CRSFEndpoint.h"

#if defined(WMEXTENSION) && defined(TARGET_RX)
#include "../../src/rx_wmextension.h"
#endif

class RXEndpoint final : public CRSFEndpoint {
public:
    RXEndpoint();
    bool handleRaw(const crsf_header_t *message) override;
    void handleMessage(const crsf_header_t *message) override;

    void registerParameters() override;
    void updateParameters() override;

#if defined(WMEXTENSION) && defined(TARGET_RX)
    const MultiSwitch& multiSwitch() const;
#endif
private:
    void luaparamMappingChannelOut(propertiesCommon *item, uint8_t arg);
    void luaparamSetFailsafe(propertiesCommon *item, uint8_t arg);

#if defined(WMEXTENSION) && defined(TARGET_RX)
    MultiSwitch msw;
#endif
};

#endif //RX_ENDPOINT_H
