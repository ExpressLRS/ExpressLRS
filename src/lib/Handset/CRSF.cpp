#include "CRSF.h"

#include "common.h"

elrsLinkStatistics_t CRSF::LinkStatistics;

/***
 * @brief: Convert `version` (string) to a integer version representation
 * e.g. "2.2.15 ISM24G" => 0x0002020f
 * Assumes all version fields are < 256, the number portion
 * MUST be followed by a space to correctly be parsed
 ***/
uint32_t CRSF::VersionStrToU32(const char *verStr)
{
    uint32_t retVal = 0;
#if !defined(FORCE_NO_DEVICE_VERSION)
    uint8_t accumulator = 0;
    char c;
    bool trailing_data = false;
    while ((c = *verStr))
    {
        ++verStr;
        // A decimal indicates moving to a new version field
        if (c == '.')
        {
            retVal = (retVal << 8) | accumulator;
            accumulator = 0;
            trailing_data = false;
        }
        // Else if this is a number add it up
        else if (c >= '0' && c <= '9')
        {
            accumulator = (accumulator * 10) + (c - '0');
            trailing_data = true;
        }
        // Anything except [0-9.] ends the parsing
        else
        {
            break;
        }
    }
    if (trailing_data)
    {
        retVal = (retVal << 8) | accumulator;
    }
    // If the version ID is < 1.0.0, we could not parse it,
    // just use the OTA version as the major version number
    if (retVal < 0x010000)
    {
        retVal = OTA_VERSION_ID << 16;
    }
#endif
    return retVal;
}

void CRSF::GetDeviceInformation(uint8_t *frame, uint8_t fieldCount)
{
    const uint8_t size = strlen(device_name)+1;
    auto *device = (deviceInformationPacket_t *)(frame + sizeof(crsf_ext_header_t) + size);
    // Packet starts with device name
    memcpy(frame + sizeof(crsf_ext_header_t), device_name, size);
    // Followed by the device
    device->serialNo = htobe32(0x454C5253); // ['E', 'L', 'R', 'S'], seen [0x00, 0x0a, 0xe7, 0xc6] // "Serial 177-714694" (value is 714694)
    device->hardwareVer = 0; // unused currently by us, seen [ 0x00, 0x0b, 0x10, 0x01 ] // "Hardware: V 1.01" / "Bootloader: V 3.06"
    device->softwareVer = htobe32(VersionStrToU32(version)); // seen [ 0x00, 0x00, 0x05, 0x0f ] // "Firmware: V 5.15"
    device->fieldCnt = fieldCount;
    device->parameterVersion = 0;
}

#if defined(CRSF_RX_MODULE)

bool CRSF::HasUpdatedUplinkPower = false;

/***
 * @brief: Call this when new uplinkPower from the TX is availble from OTA instead of setting directly
 */
void CRSF::updateUplinkPower(uint8_t uplinkPower)
{
    if (uplinkPower != LinkStatistics.uplink_TX_Power)
    {
        LinkStatistics.uplink_TX_Power = uplinkPower;
        HasUpdatedUplinkPower = true;
    }
}

/***
 * @brief: Returns true if HasUpdatedUplinkPower and clears the flag
 */
bool CRSF::clearUpdatedUplinkPower()
{
    bool retVal = HasUpdatedUplinkPower;
    HasUpdatedUplinkPower = false;
    return retVal;
}

#endif // CRSF_RX_MODULE
