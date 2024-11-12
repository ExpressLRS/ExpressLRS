#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)
#include "devSerialUpdate.h"
#include "common.h"
#include "hwTimer.h"
#include "POWERMGNT.h"
#include "devVTXSPI.h"
#include "devMSPVTX.h"

extern void start_esp_upload();
extern void stub_handle_rx_byte(char byte);

static bool running = false;

static void initialize()
{
    running = true;
}

static int event()
{
    if (connectionState == serialUpdate && running)
    {
        running = false;
        hwTimer::stop();
        disableVTxSpi();
        disableMspVtx();
        POWERMGNT::setPower(MinPower);
        Radio.End();
        return DURATION_IMMEDIATELY;
    }
    return DURATION_IGNORE;
}

static int timeout()
{
    if (connectionState != serialUpdate)
    {
        return DURATION_NEVER;
    }

    start_esp_upload();
    while (true)
    {
        uint8_t buf[64];
        int count = Serial.read(buf, sizeof(buf));
        for (int i=0 ; i<count ; i++)
        {
            stub_handle_rx_byte(buf[i]);
        }
    }
}

device_t SerialUpdate_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};
#endif