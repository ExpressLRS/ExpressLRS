#include <Arduino.h>
#include "targets.h"
#include "telemetry.h"
#include "xmodem_server.h"

extern bool LED;
extern Telemetry telemetry;

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#else
#error "Radio configuration is not valid!"
#endif

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

void esp8266_xmodem_tx(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
    (void)xdm;
    Serial.write(byte);
}

void esp8266_xmodem_updater()
{
    Radio.End();

    for (uint8_t i = 0; i < 5; i++)
    {
        Serial.println("ESP8266 XMODEM BOOTLOADER"); // tell script we got into bootloader
    }

    //uint32_t uploadSize = telemetry.BootloaderFilesize() ; // didn't seem to work, assign max size instead
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

    uint8_t md5_bytes[16] = {0};
    char md5_str[32] = "";
    telemetry.BootloaderMD5hash(md5_bytes);
    array_to_string(md5_bytes, sizeof(md5_bytes), md5_str);

    if (!Update.setMD5(md5_str))
    {
        Serial.println("MD5 ERROR");
        Update.printError(Serial);
        return;
    }

    if (!Update.begin(maxSketchSpace, U_FLASH))
    { //start with max available size
        Serial.println("UPDATE FAILURE (BEGIN)");
        Update.printError(Serial);
        return;
    }

    struct xmodem_server xdm;
    xmodem_server_init(&xdm, esp8266_xmodem_tx, NULL);

    while (!xmodem_server_is_done(&xdm))
    {
        uint8_t resp[XMODEM_MAX_PACKET_SIZE] = {0};
        uint32_t block_nr = 0;
        uint32_t rx_data_len = 0;

        if (Serial.available())
        {
            xmodem_server_rx_byte(&xdm, Serial.read());
        }

        rx_data_len = xmodem_server_process(&xdm, resp, &block_nr, millis());

        if (rx_data_len > 0)
        {
            if ((uint32_t)Update.write(resp, rx_data_len) == rx_data_len)
            {
                #ifdef GPIO_PIN_LED
                digitalWrite(GPIO_PIN_LED, LED ^ GPIO_LED_RED_INVERTED);
                #endif
                LED = !LED;
            }
        }
    }

    delay(100);

    if (xmodem_server_get_state(&xdm) != XMODEM_STATE_SUCCESSFUL)
    {
        Serial.println("UPDATE FAILURE (XMODEM)");
        ESP.restart();
    }

    if (Update.end(true)) // force update since uploadSize < maxSketchSpace
    {
        Serial.println("UPDATE SUCCESS");
        ESP.restart();
    }
    else
    {
        Serial.println("UPDATE FAILURE (GENERIC)");
    }
}