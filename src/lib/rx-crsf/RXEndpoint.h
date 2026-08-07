#ifndef RX_ENDPOINT_H
#define RX_ENDPOINT_H
#include "RxTxEndpoint.h"

class RXEndpoint final : public RxTxEndpoint {
public:
    RXEndpoint();
    bool handleRaw(const crsf_header_t *message) override;
    void handleMessage(const crsf_header_t *message) override;

    void registerParameters() override;
    void updateParameters() override;

private:
    void luaparamMappingChannelOut(propertiesCommon *item, uint8_t arg);
    void luaparamSetFailsafe(propertiesCommon *item, uint8_t arg);
#if defined(GYRO_SUPPORT)
    // Commands
    void luaparamGyroQuickPreset(propertiesCommon *item, uint8_t arg);
    void luaparamGyroCalibration(propertiesCommon *item, uint8_t arg);
    void luaparamGyroOrientationCal(propertiesCommon *item, uint8_t arg);
    void luaparamGyroStickCal(propertiesCommon *item, uint8_t arg);

    // Selections
    void luaparamGyroInputCh_Select(propertiesCommon *item, uint8_t arg);
    void luaparamGyroOutputCh_Select(propertiesCommon *item, uint8_t arg);
    void luaparamGyroPIG_AxisSelect(propertiesCommon *item, uint8_t arg);
    void luaparamGyroFMode_Select(propertiesCommon *item, uint8_t arg);
#endif
};

#endif //RX_ENDPOINT_H
