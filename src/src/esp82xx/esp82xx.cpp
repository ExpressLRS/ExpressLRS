#include "targets.h"
#include "debug.h"
#include <Arduino.h>

bool webUpdateMode = false;

void platform_setup(void)
{
}

void platform_loop(bool connected)
{
    if (webUpdateMode)
    {
        HandleWebUpdate();
        if (millis() > WEB_UPDATE_LED_FLASH_INTERVAL + webUpdateLedFlashIntervalLast)
        {
            digitalWrite(GPIO_PIN_LED, LED);
            LED = !LED;
            webUpdateLedFlashIntervalLast = millis();
        }
    }
}

void platform_connection_state(bool connected)
{
    (void)connected;
}
