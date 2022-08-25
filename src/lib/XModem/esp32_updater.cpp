#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)
#include "common.h"
#include "hwTimer.h"
#include "POWERMGNT.h"
#include "devVTXSPI.h"

#include "telemetry.h"
#include "xmodem_server.h"

#include <Update.h>


void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0f;
        byte nib2 = (array[i] >> 0) & 0x0f;
        buffer[i*2+0] = nib1  < 0xa ? '0' + nib1  : 'a' + nib1  - 0xa;
        buffer[i*2+1] = nib2  < 0xa ? '0' + nib2  : 'a' + nib2  - 0xa;
    }
    buffer[len*2] = '\0';
}

void esp32_xmodem_tx(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
    (void)xdm;
    Serial.write(byte);
}

void esp32_xmodem_updater()
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
        return;
    }

    struct xmodem_server *xdm = new xmodem_server();
    xmodem_server_init(xdm, esp32_xmodem_tx, NULL);

    uint8_t *resp = new uint8_t[XMODEM_MAX_PACKET_SIZE];
    while (!xmodem_server_is_done(xdm))
    {
        if (Serial.available())
        {
            xmodem_server_rx_byte(xdm, Serial.read());
        }

        uint32_t block_nr = 0;
        uint32_t rx_data_len = xmodem_server_process(xdm, resp, &block_nr, millis());

        if (rx_data_len > 0)
        {
            if ((uint32_t)Update.write(resp, rx_data_len) != rx_data_len)
            {
                break;
            }
            delay(1);
        }
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
}
#endif