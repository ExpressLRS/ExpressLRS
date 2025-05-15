#include "RXEndpoint.h"
#include "device.h"

extern RXEndpoint crsfReceiver;

static int event()
{
    crsfReceiver.updateParameters();
    return DURATION_NEVER;
}

static int start()
{
    crsfReceiver.registerParameters();
    event();
    return DURATION_NEVER;
}

device_t RXLUA_device = {
  .initialize = nullptr,
  .start = start,
  .event = event,
  .timeout = nullptr,
  .subscribe = EVENT_ALL
};
