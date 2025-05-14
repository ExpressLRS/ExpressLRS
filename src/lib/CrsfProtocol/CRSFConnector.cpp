#include "targets.h"

#include "CRSFConnector.h"
#include "logging.h"

void CRSFConnector::addDevice(const crsf_addr_e device_id)
{
    devices.insert(device_id);
}

bool CRSFConnector::forwardsTo(const crsf_addr_e device_id)
{
    return devices.find(device_id) != devices.end();
}

void CRSFConnector::debugCRSF(const char *str, const crsf_header_t *message)
{
    DBGLN(str);
    DBGLN("dev:  %x", message->device_addr);
    DBGLN("size: %x", message->frame_size);
    DBGLN("type: %x", message->type);
    if (message->type >= CRSF_FRAMETYPE_DEVICE_PING)
    {
        const auto ext = (crsf_ext_header_t *)message;
        DBGLN("dest: %x", ext->dest_addr);
        DBGLN("orig: %x", ext->orig_addr);
    }
}
