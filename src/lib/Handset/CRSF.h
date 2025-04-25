#ifndef H_CRSF
#define H_CRSF

#include "crsf_protocol.h"

class CRSF
{
public:
    static void GetDeviceInformation(uint8_t *frame, uint8_t fieldCount);
    static uint32_t VersionStrToU32(const char *verStr);

#if defined(CRSF_RX_MODULE)
    static void updateUplinkPower(uint8_t uplinkPower);
    static bool clearUpdatedUplinkPower();

private:
    static bool HasUpdatedUplinkPower;
#endif

private:
};

#endif