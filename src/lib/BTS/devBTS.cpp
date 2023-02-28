#include "devBTS.h"

#if defined(PLATFORM_ESP32)

#include "common.h"
#include "CRSF.h"
#include "logging.h"
#include "BluetoothSerial.h"
#include "config.h"

extern TxConfig config;

BluetoothSerial SerialBT;
bool started = false;

static int event()
{
    // Shutdown if not enabled, ignore if in init mode
    if ( (config.GetBTSerial() == false) || (connectionState == bleJoystick) )
    {
        CRSF::PortSecondary = nullptr;
        SerialBT.end();
        started = false;
        DBGLN("Shutting down BT Serial!");
        return DURATION_NEVER;
    }

    // Start BT stack if not started yet and service is enabled via LUA
    if (config.GetBTSerial() == true && !started)
    {
        started = true;
        SerialBT.begin("ELRS Telemetry");
        CRSF::PortSecondary = &SerialBT;

        DBGLN("Starting BT Serial!");
        return DURATION_NEVER;
    }

    return DURATION_NEVER;
}

device_t BTS_device = {
  .initialize = NULL,
  .start = NULL,
  .event = event,
  .timeout = NULL
};

#endif
