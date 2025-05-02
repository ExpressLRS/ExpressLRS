#include "TXModuleEndpoint.h"
#include "common.h"
#include "device.h"

extern TXModuleEndpoint crsfTransmitter;

static int event()
{
  if (connectionState > FAILURE_STATES)
  {
    return DURATION_NEVER;
  }
  crsfTransmitter.updateParameters();
  return DURATION_NEVER;
}

static int start()
{
  if (connectionState < FAILURE_STATES)
  {
    event();
  }
  return DURATION_NEVER;
}

device_t TXLUA_device = {
  .initialize = nullptr,
  .start = start,
  .event = event,
  .timeout = nullptr,
  .subscribe = EVENT_ALL
};
