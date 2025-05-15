#ifndef TX_MODULE_ENDPOINT_H
#define TX_MODULE_ENDPOINT_H
#include "CRSFEndpoint.h"

enum warningFlags
{
    // bit 0 and 1 are status flags, show up as the little icon in the lua top right corner
    LUA_FLAG_CONNECTED = 0,
    LUA_FLAG_STATUS1,
    // bit 2,3,4 are warning flags, change the tittle bar every 0.5s
    LUA_FLAG_MODEL_MATCH,
    LUA_FLAG_ISARMED,
    LUA_FLAG_WARNING1,
    // bit 5,6,7 are critical warning flag, block the lua screen until user confirm to suppress the warning.
    LUA_FLAG_ERROR_CONNECTED,
    LUA_FLAG_ERROR_BAUDRATE,
    LUA_FLAG_CRITICAL_WARNING2,
};

class TXModuleEndpoint final : public CRSFEndpoint {
public:
    TXModuleEndpoint() : CRSFEndpoint(CRSF_ADDRESS_CRSF_TRANSMITTER) {}
    ~TXModuleEndpoint() override = default;

    void begin();

    bool handleRaw(const crsf_header_t *message) override;
    void handleMessage(const crsf_header_t *message) override;
    void RcPacketToChannelsData(const crsf_header_t *message);

    void updateFolderNames();
    void registerParameters() override;
    void updateParameters() override;

    uint8_t modelId = 0; // The model ID as received from the Transmitter

protected:
    void devicePingCalled() override;
    void updateModelID();

    void supressCriticalErrors();
    void setWarningFlag(warningFlags flag, bool value);
    void sendELRSstatus(crsf_addr_e origin);

private:
    bool armCmd = false; // Arm command from handset either via ch5 or arm message
    bool lastArmCmd = false;

    char luaBadGoodString[10] {};
    uint8_t luaWarningFlags = 0b00000000; //8 flag, 1 bit for each flag. set the bit to 1 to show specific warning. 3 MSB is for critical flag

    void handleWifiBle(propertiesCommon *item, uint8_t arg);
    void handleSimpleSendCmd(propertiesCommon *item, uint8_t arg);
    void updateTlmBandwidth();
    void updateBackpackOpts();
};

#endif //TX_MODULE_ENDPOINT_H
