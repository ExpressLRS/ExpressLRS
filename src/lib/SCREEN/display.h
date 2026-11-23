#pragma once

#include "targets.h"
#include "menu.h"
#include "uimodel.h"

class Print;

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

    // Animation/refresh hook, called from the screen poll every ~20ms
    virtual void tick(uint32_t now) {}
    // Multi-page link statistics (page 0 is displayLinkstats)
    virtual void displayLinkstatsPage(uint8_t page) { displayLinkstats(); }
    // Called when a value picker opens: selectable range and the saved value
    virtual void setValueBounds(int min, int max, int saved) {}
    // True when the value picker is laid out horizontally: LEFT/RIGHT then step
    // the value and a long LEFT goes back
    virtual bool horizontalValuePicker() { return false; }
    // Raw framebuffer dump for remote screenshots (debug builds)
    // Rows are prefixed "NNN:" so a host can re-request damaged ones (first/count)
    virtual void dumpFrame(Print &out, int first = 0, int count = -1) {}

    static int getValueCount(menu_item_t menu);
    static const char *getValue(menu_item_t menu, uint8_t value_index);
    static const char *getValue(int menu, int value_index) { return getValue((menu_item_t)menu, (uint8_t)value_index); }
    static const char *menuString(int menu, int line) { return main_menu_strings[menu][line]; }

protected:
    // Copies the idle-screen parameters into the shared UI model
    static void dumpRows(Print &out, const char *header, const uint8_t *buf, int rowBytes, int rows, int first, int count);
    static void writeAll(Print &out, const char *buf, size_t len);
    void storeIdle(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, bool dynamic, uint8_t running_power_index, uint8_t temperature);
    static const char *message_string[];
    static const char *main_menu_strings[][2];
};
