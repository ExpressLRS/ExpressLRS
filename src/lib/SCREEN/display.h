#pragma once

#include "targets.h"
#include "menu.h"

#define CHANGED_TEMP bit(0)
#define CHANGED_RATE bit(1)
#define CHANGED_POWER bit(2)
#define CHANGED_TELEMETRY bit(3)
#define CHANGED_MOTION bit(4)
#define CHANGED_FAN bit(5)
#define CHANGED_ALL 0xFF

typedef enum fsm_state_s menu_item_t;

typedef enum message_index_e {
    MSG_NONE,
    MSG_CONNECTED,
    MSG_ARMED,
    MSG_MISMATCH,
    MSG_ERROR,
    MSG_INVALID
} message_index_t;

typedef enum
{
    SCREEN_BACKLIGHT_ON = 0,
    SCREEN_BACKLIGHT_OFF = 1
} screen_backlight_t;

class Display
{
public:
    virtual void init() = 0;
    virtual void doScreenBackLight(screen_backlight_t state) = 0;
    virtual void printScreenshot() = 0;

    virtual void displaySplashScreen() = 0;
    virtual void displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index) = 0;
    virtual void displayMainMenu(menu_item_t menu) = 0;
    virtual void displayValue(menu_item_t menu, uint8_t value_index) = 0;
    virtual void displayBLEConfirm() = 0;
    virtual void displayBLEStatus() = 0;
    virtual void displayBindConfirm() = 0;
    virtual void displayBindStatus() = 0;
    virtual void displayWiFiConfirm() = 0;
    virtual void displayWiFiStatus() = 0;
    virtual void displayRunning() = 0;
    virtual void displaySending() = 0;
    virtual void displayLinkstats() = 0;

    int getValueCount(menu_item_t menu);
    const char *getValue(menu_item_t menu, uint8_t value_index);

protected:
    static const char *message_string[];
    static const char *main_menu_strings[][2];
};
