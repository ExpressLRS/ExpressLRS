#pragma once

#include "targets.h"
#include "menu.h"

#ifndef USE_OLED_I2C
#define OPT_USE_OLED_I2C false
#elif !defined(OPT_USE_OLED_I2C)
#define OPT_USE_OLED_I2C true
#endif
#ifndef USE_OLED_SPI
#define OPT_USE_OLED_SPI false
#elif !defined(OPT_USE_OLED_SPI)
#define OPT_USE_OLED_SPI true
#endif
#ifndef USE_OLED_SPI_SMALL
#define OPT_USE_OLED_SPI_SMALL false
#elif !defined(OPT_USE_OLED_SPI_SMALL)
#define OPT_USE_OLED_SPI_SMALL true
#endif
#ifndef OLED_REVERSED
#define OPT_OLED_REVERSED false
#elif !defined(OPT_OLED_REVERSED)
#define OPT_OLED_REVERSED true
#endif
#ifndef HAS_TFT_SCREEN
#define OPT_HAS_TFT_SCREEN false
#elif !defined(OPT_HAS_TFT_SCREEN)
#define OPT_HAS_TFT_SCREEN true
#endif


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
    static void init();
    static void doScreenBackLight(screen_backlight_t state);
    static void printScreenshot();

    static void displaySplashScreen();
    static void displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index);
    static void displayMainMenu(menu_item_t menu);
    static void displayValue(menu_item_t menu, uint8_t value_index);
    static void displayBLEConfirm();
    static void displayBLEStatus();
    static void displayBindConfirm();
    static void displayBindStatus();
    static void displayWiFiConfirm();
    static void displayWiFiStatus();
    static void displayRunning();

    static int getValueCount(menu_item_t menu);
    static const char *getValue(menu_item_t menu, uint8_t value_index);

protected:
    static const char *message_string[];
    static const char *main_menu_strings[][2];
};
