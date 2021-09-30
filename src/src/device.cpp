#include <stdint.h>

#include "device.h"
#include "common.h"
#include "helpers.h"

extern device_t LED_device;
extern device_t WIFI_device;
extern device_t RGB_device;
#ifdef TARGET_TX
extern device_t LUA_device;
extern device_t BLE_device;
extern device_t OLED_device;
extern device_t Buzzer_device;
#endif

device_t *ui_devices[] = {
  &LED_device,
  &RGB_device,
#ifdef TARGET_TX
  &LUA_device,
  &BLE_device,
  &OLED_device,
  &Buzzer_device,
#endif
  &WIFI_device,
};

unsigned long ui_device_timeout[ARRAY_SIZE(ui_devices)] = {0};
unsigned long nextDeviceTimeout = 0;

static connectionState_e lastConnectionState = disconnected;

void initDevices()
{
    for(size_t i=0 ; i<ARRAY_SIZE(ui_devices) ; i++) {
        if (ui_devices[i]->initialize) {
            (ui_devices[i]->initialize)();
        }
    }
}

void startDevices()
{
    unsigned long now = millis();
    for(size_t i=0 ; i<ARRAY_SIZE(ui_devices) ; i++)
    {
        if (ui_devices[i]->start)
        {
            int delay = (ui_devices[i]->start)();
            ui_device_timeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            nextDeviceTimeout = min(nextDeviceTimeout, ui_device_timeout[i]);
        }
    }
}

void handleDevices(unsigned long now, bool eventFired, std::function<void ()> setSpam)
{
    for(size_t i=0 ; i<ARRAY_SIZE(ui_devices) ; i++)
    {
        if ((eventFired || lastConnectionState != connectionState) && ui_devices[i]->event)
        {
            int delay = (ui_devices[i]->event)(setSpam);
            if (delay != DURATION_IGNORE)
            {
                ui_device_timeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
                nextDeviceTimeout = min(nextDeviceTimeout, ui_device_timeout[i]);
            }
        }
    }
    lastConnectionState = connectionState;
    for(size_t i=0 ; i<ARRAY_SIZE(ui_devices) ; i++)
    {
        if (now > ui_device_timeout[i] && ui_devices[i]->timeout)
        {
            int delay = (ui_devices[i]->timeout)(setSpam);
            ui_device_timeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            nextDeviceTimeout = min(nextDeviceTimeout, ui_device_timeout[i]);
        }
    }
}
