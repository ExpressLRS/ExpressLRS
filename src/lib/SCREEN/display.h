#pragma once

#include "targets.h"

#define MSG_NONE_INDEX 0
#define MSG_CONNECTED_INDEX 1
#define MSG_ARMED_INDEX 2
#define MSG_MISMATCH_INDEX 3

typedef enum menu_item_e {
    MENU_PACKET,
    MENU_POWER,
    MENU_TELEMETRY,
    MENU_POWERSAVE,
    MENU_SMARTFAN,
    MENU_BIND,
    MENU_WIFI
} menu_item_t;

typedef enum
{
    SCREEN_BACKLIGHT_ON = 0,
    SCREEN_BACKLIGHT_OFF = 1
} screen_backlight_t;

typedef struct item_values_s {
    const char **values;
    const uint8_t count;
} item_values_t;

class Display
{
public:
    virtual void init() = 0;
    virtual void doScreenBackLight(screen_backlight_t state) = 0;

    virtual void displaySplashScreen() = 0;
    virtual void displayIdleScreen(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t message_index) = 0;
    virtual void displayMainMenu(menu_item_t menu) = 0;
    virtual void displayValue(menu_item_t menu, uint8_t value_index) = 0;
    virtual void displayWiFiConfirm() = 0;
    virtual void displayWiFiStatus() = 0;
    virtual void displayBindConfirm() = 0;
    virtual void displayBindStatus() = 0;

    static const item_values_t value_sets[];

protected:
    static const char *message_string[];
    static const char *main_menu_line_1[];
    static const char *main_menu_line_2[];
};
