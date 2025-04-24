#include "CRSFConnector.h"

void CRSFConnector::addDevice(const crsf_addr_e device_id)
{
    devices.insert(device_id);
}

bool CRSFConnector::forwardsTo(const crsf_addr_e device_id)
{
    return devices.find(device_id) != devices.end();
}
