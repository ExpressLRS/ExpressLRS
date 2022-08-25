#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)
#include <Update.h>

#include "devSerialUpdate.h"
#include "common.h"
#include "hwTimer.h"
#include "POWERMGNT.h"
#include "devVTXSPI.h"

#include "telemetry.h"
#include "xmodem_server.h"


static struct xmodem_server *xdm = nullptr;
static uint8_t *resp;

static void initialize()
{
}

static void esp32_xmodem_tx(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
    (void)xdm;
    Serial.write(byte);
}

static int process()
{
    uint32_t block_nr = 0;
    uint32_t rx_data_len = xmodem_server_process(xdm, resp, &block_nr, millis());
    if (rx_data_len > 0)
    {
        if ((uint32_t)Update.write(resp, rx_data_len) != rx_data_len)
        {
            Serial.println("UPDATE FAILURE (GENERIC)");
            Update.printError(Serial);
            ESP.restart();
            return false;
        }
    }
    return true;
}

static int event()
{
    if (connectionState == serialUpdate && xdm == nullptr)
    {
        hwTimer::stop();
#ifdef HAS_VTX_SPI
        VTxOutputMinimum();
#endif
        POWERMGNT::setPower(MinPower);
        Radio.End();

        if (!Update.begin()) //start with max available size
        {
            Serial.println("UPDATE FAILURE (BEGIN)");
            Update.printError(Serial);
            return DURATION_NEVER;
        }

        xdm = new xmodem_server();
        xmodem_server_init(xdm, esp32_xmodem_tx, NULL);

        resp = new uint8_t[XMODEM_MAX_PACKET_SIZE];
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

    if (!xmodem_server_is_done(xdm))
    {
        int count = Serial.available();
        while (count--)
        {
            xmodem_server_rx_byte(xdm, Serial.read());
            if (!process())
            {
                return DURATION_NEVER;
            }
        }
        if (!process())
        {
            return DURATION_NEVER;
        }
        return DURATION_IMMEDIATELY;
    }

    delay(100);

    if (xmodem_server_get_state(xdm) != XMODEM_STATE_SUCCESSFUL)
    {
        Serial.println("UPDATE FAILURE (XMODEM)");
    }
    else if (Update.end(true)) // force update since uploadSize < maxSketchSpace
    {
        Serial.println("UPDATE SUCCESS");
    }
    else
    {
        Serial.println("UPDATE FAILURE (GENERIC)");
        Update.printError(Serial);
    }
    ESP.restart();
    return DURATION_NEVER;
}

device_t SerialUpdate_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};
#endif