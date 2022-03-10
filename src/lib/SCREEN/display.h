#pragma once

#include "targets.h"
#include "menu.h"

#define CHANGED_MESSAGE bit(0)
#define CHANGED_RATE bit(1)
#define CHANGED_POWER bit(2)
#define CHANGED_TELEMETRY bit(3)
#define CHANGED_MOTION bit(4)
#define CHANGED_FAN bit(5)

typedef enum fsm_state_s menu_item_t;

typedef enum message_index_e {
    MSG_NONE,
    MSG_CONNECTED,
    MSG_ARMED,
    MSG_MISMATCH,
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
    void init();
    void doScreenBackLight(screen_backlight_t state);
    void printScreenshot();

    static void displaySplashScreen();
    static void displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index);
    static void displayMainMenu(menu_item_t menu);
    static void displayValue(menu_item_t menu, uint8_t value_index);
    static void displayBLEConfirm();
    static void displayBLEStatus();
    static void displayWiFiConfirm();
    static void displayWiFiStatus();
    static void displayBindConfirm();
    static void displayBindStatus();

    static int getValueCount(menu_item_t menu);
    static const char *getValue(menu_item_t menu, uint8_t value_index);

protected:
    static const char *message_string[];
    static const char *main_menu_strings[][2];
};
