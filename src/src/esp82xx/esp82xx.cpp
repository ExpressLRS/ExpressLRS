#include "targets.h"
#include "debug_elrs.h"
#include "button.h"
#include "common.h"
#include "ESP8266_WebUpdate.h"

#include <Arduino.h>

#define WEB_UPDATE_LED_FLASH_INTERVAL 25

uint32_t webUpdateLedFlashIntervalNext = 0;

void beginWebsever(void)
{
    if (connectionState != STATE_disconnected)
        return;

    forced_stop();

    BeginWebUpdate();
    connectionState = STATE_fw_upgrade;
    webUpdateLedFlashIntervalNext = 0;
}

#ifdef GPIO_PIN_BUTTON
#include "button.h"
Button button;

void button_event_short(uint32_t ms)
{
    (void)ms;
    forced_start();
}

void button_event_long(uint32_t ms)
{
    if (ms > BUTTON_RESET_INTERVAL_RX)
        ESP.restart();
    else if (ms > WEB_UPDATE_PRESS_INTERVAL)
        beginWebsever();
}
#endif

void platform_setup(void)
{
    /* Force WIFI off until it is realy needed */
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();

#ifdef GPIO_PIN_LED
    pinMode(GPIO_PIN_LED, OUTPUT);
#endif
#ifdef GPIO_PIN_BUTTON
    button.buttonShortPress = button_event_short;
    button.buttonLongPress = button_event_long;
    button.init(GPIO_PIN_BUTTON, true);
#endif
}

void platform_loop(int state)
{
    uint32_t now = millis();
    if (state == STATE_fw_upgrade)
    {
        HandleWebUpdate();
        if (WEB_UPDATE_LED_FLASH_INTERVAL < (now - webUpdateLedFlashIntervalNext))
        {
#ifdef GPIO_PIN_LED
            // toggle led
            digitalWrite(GPIO_PIN_LED, !digitalRead(GPIO_PIN_LED));
#endif
            webUpdateLedFlashIntervalNext = now;
        }
    }
    else
    {
#ifdef GPIO_PIN_BUTTON
        button.handle();
#endif
    }
}

void platform_connection_state(int state)
{
#ifdef Auto_WiFi_On_Boot
    if (state == STATE_search_iteration_done && millis() < 30000)
    {
        beginWebsever();
    }
#endif /* Auto_WiFi_On_Boot */
}

void platform_set_led(bool state)
{
#ifdef GPIO_PIN_LED
    digitalWrite(GPIO_PIN_LED, (uint32_t)(!state)); // Invert led
#endif
}

void platform_restart(void)
{
    ESP.restart();
}

void platform_wd_feed(void)
{
}
