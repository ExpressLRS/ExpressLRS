#include "RXOTAConnector.h"

#include "telemetry.h"

extern Telemetry telemetry;

RXOTAConnector::RXOTAConnector()
{
    addDevice(CRSF_ADDRESS_CRSF_TRANSMITTER);
}

void RXOTAConnector::forwardMessage(const crsf_header_t *message)
{
    telemetry.AppendTelemetryPackage((uint8_t *)message);
}