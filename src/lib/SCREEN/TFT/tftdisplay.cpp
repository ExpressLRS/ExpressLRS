#ifdef HAS_TFT_SCREEN

#include <TFT_eSPI.h>

#include "display.h"

#include "logos.h"
#include "options.h"
#include "logging.h"

#include "WiFi.h"
extern WiFiMode_t wifiMode;

TFT_eSPI tft = TFT_eSPI();

const uint16_t *main_menu_icons[] = {
    elrs_rate,
    elrs_power,
    elrs_ratio,
    elrs_motion,
    elrs_fan,
    elrs_vtx,
    elrs_joystick,
    elrs_bind,
    elrs_wifimode,

    elrs_power,
    elrs_power,

    elrs_vtx,
    elrs_vtx,
    elrs_vtx,
    elrs_vtx,
    elrs_vtx,

    elrs_updatefw,
    elrs_rxwifi,
    elrs_backpack,
    elrs_vrxwifi
};

// Hex color code to 16-bit rgb:
// color = 0x96c76f
// rgb_hex = ((((color&0xFF0000)>>16)&0xf8)<<8) + ((((color&0x00FF00)>>8)&0xfc)<<3) + ((color&0x0000FF)>>3)
constexpr uint16_t elrs_banner_bgColor[] = {
    0x4315, // MSG_NONE          => #4361AA (ELRS blue)
    0x9E2D, // MSG_CONNECTED     => #9FC76F (ELRS green)
    0xAA08, // MSG_ARMED         => #AA4343 (red)
    0xF501  // MSG_MISMATCH      => #F0A30A (amber)
};

#define SCREEN_X    TFT_HEIGHT
#define SCREEN_Y    TFT_WIDTH

//see #define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define SCREEN_SMALL_FONT_SIZE      8
#define SCREEN_SMALL_FONT           1

//see #define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define SCREEN_NORMAL_FONT_SIZE     16
#define SCREEN_NORMAL_FONT          2

//see #define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define SCREEN_LARGE_FONT_SIZE      26
#define SCREEN_LARGE_FONT           4

//ICON SIZE Definition
#define SCREEN_LARGE_ICON_SIZE      60

//GAP Definition
#define SCREEN_CONTENT_GAP          10
#define SCREEN_FONT_GAP             5

//INIT LOGO PAGE Definition
#define INIT_PAGE_LOGO_X            SCREEN_X
#define INIT_PAGE_LOGO_Y            53
#define INIT_PAGE_FONT_PADDING      3
#define INIT_PAGE_FONT_START_X      SCREEN_FONT_GAP
#define INIT_PAGE_FONT_START_Y      INIT_PAGE_LOGO_Y + (SCREEN_Y - INIT_PAGE_LOGO_Y - SCREEN_NORMAL_FONT_SIZE)/2

//IDLE PAGE Definition
#define IDLE_PAGE_START_X   SCREEN_CONTENT_GAP
#define IDLE_PAGE_START_Y   0

#define IDLE_PAGE_STAT_START_X  SCREEN_X/2 + SCREEN_CONTENT_GAP
#define IDLE_PAGE_STAT_Y_GAP    (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE * 3)/4

#define IDLE_PAGE_RATE_START_Y  IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_POWER_START_Y IDLE_PAGE_RATE_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_RATIO_START_Y IDLE_PAGE_POWER_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP

//MAIN PAGE Definition
#define MAIN_PAGE_ICON_START_X  SCREEN_CONTENT_GAP
#define MAIN_PAGE_ICON_START_Y  (SCREEN_Y -  SCREEN_LARGE_ICON_SIZE)/2

#define MAIN_PAGE_WORD_START_X  MAIN_PAGE_ICON_START_X + SCREEN_LARGE_ICON_SIZE + SCREEN_CONTENT_GAP
#define MAIN_PAGE_WORD_START_Y1 (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE*2 - SCREEN_FONT_GAP)/2
#define MAIN_PAGE_WORD_START_Y2 MAIN_PAGE_WORD_START_Y1 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP

//Sub Function Definiton
#define SUB_PAGE_VALUE_START_X  SCREEN_CONTENT_GAP
#define SUB_PAGE_VALUE_START_Y  (SCREEN_Y - SCREEN_LARGE_FONT_SIZE - SCREEN_NORMAL_FONT_SIZE - SCREEN_CONTENT_GAP)/2

#define SUB_PAGE_TIPS_START_X  SCREEN_CONTENT_GAP
#define SUB_PAGE_TIPS_START_Y  SCREEN_Y - SCREEN_NORMAL_FONT_SIZE - SCREEN_CONTENT_GAP

//Sub WIFI Mode & Bind Confirm Definiton
#define SUB_PAGE_ICON_START_X  0
#define SUB_PAGE_ICON_START_Y  (SCREEN_Y -  SCREEN_LARGE_ICON_SIZE)/2

#define SUB_PAGE_WORD_START_X   SUB_PAGE_ICON_START_X + SCREEN_LARGE_ICON_SIZE
#define SUB_PAGE_WORD_START_Y1  (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE*3 - SCREEN_FONT_GAP*2)/2
#define SUB_PAGE_WORD_START_Y2  SUB_PAGE_WORD_START_Y1 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP
#define SUB_PAGE_WORD_START_Y3  SUB_PAGE_WORD_START_Y2 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP

//Sub Binding Definiton
#define SUB_PAGE_BINDING_WORD_START_X   0
#define SUB_PAGE_BINDING_WORD_START_Y   (SCREEN_Y -  SCREEN_LARGE_FONT_SIZE)/2

void Display::init()
{
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    doScreenBackLight(SCREEN_BACKLIGHT_ON);
}

void Display::doScreenBackLight(screen_backlight_t state)
{
    #ifdef TFT_BL
    digitalWrite(TFT_BL, state);
    #endif
}

void Display::printScreenshot()
{
    DBGLN("Unimplemented");
}

static void displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y,
                                            int font_size, int font_type, String font_string,
                                            uint16_t fgColor, uint16_t bgColor)
{
    tft.fillRect(font_start_x, font_start_y, font_end_x - font_start_x, font_size, bgColor);

    int start_pos = font_start_x + (font_end_x - font_start_x -  tft.textWidth(font_string, font_type))/2;
    tft.setCursor(start_pos, font_start_y, font_type);

    tft.setTextColor(fgColor, bgColor);
    tft.print(font_string);
}


void Display::displaySplashScreen()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(0, 0, INIT_PAGE_LOGO_X, INIT_PAGE_LOGO_Y, vendor_logo);

    tft.fillRect(SCREEN_FONT_GAP, INIT_PAGE_FONT_START_Y - INIT_PAGE_FONT_PADDING,
                    SCREEN_X - SCREEN_FONT_GAP*2, SCREEN_NORMAL_FONT_SIZE + INIT_PAGE_FONT_PADDING*2, TFT_BLACK);

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%s  ELRS-%.6s", HARDWARE_VERSION, version);
    displayFontCenter(INIT_PAGE_FONT_START_X, SCREEN_X - INIT_PAGE_FONT_START_X, INIT_PAGE_FONT_START_Y,
                        SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        String(buffer), TFT_WHITE, TFT_BLACK);
}

void Display::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    if (changed == 0xFF)
    {
        // Everything has changed! So clear the right side
        tft.fillRect(SCREEN_X/2, 0, SCREEN_X/2, SCREEN_Y, TFT_WHITE);
    }

    if (changed & CHANGED_MESSAGE)
    {
        // Left side logo, version, and temp
        tft.fillRect(0, 0, SCREEN_X/2, SCREEN_Y, elrs_banner_bgColor[message_index]);
        tft.drawBitmap(IDLE_PAGE_START_X, IDLE_PAGE_START_Y, elrs_banner_bmp, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE,
                        TFT_WHITE);

        // Update the temperature
        char buffer[20];
        // \367 = (char)247 = degree symbol
        snprintf(buffer, sizeof(buffer), "%.6s %02d\367C", version, temperature);
        displayFontCenter(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2,
                            SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT,
                            String(buffer), TFT_WHITE, elrs_banner_bgColor[message_index]);
    }

    // The Radio Params right half of the screen
    uint16_t text_color = (message_index == MSG_ARMED) ? TFT_DARKGREY : TFT_BLACK;

    if (changed & CHANGED_RATE)
    {
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATE_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            getValue(STATE_PACKET, rate_index), text_color, TFT_WHITE);
    }

    if (changed & CHANGED_POWER)
    {
        String power = getValue(STATE_POWER, dynamic ? running_power_index : power_index);
        if (dynamic)
        {
            power += " *";
        }
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_POWER_START_Y, SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            power, text_color, TFT_WHITE);
    }

    if (changed & CHANGED_TELEMETRY)
    {
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATIO_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            getValue(STATE_TELEMETRY, ratio_index), text_color, TFT_WHITE);
    }
}

void Display::displayMainMenu(menu_item_t menu)
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(MAIN_PAGE_ICON_START_X, MAIN_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, main_menu_icons[menu]);

    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
        main_menu_strings[menu][0], TFT_BLACK, TFT_WHITE);

    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
        main_menu_strings[menu][1], TFT_BLACK, TFT_WHITE);
}

void Display::displayValue(menu_item_t menu, uint8_t value_index)
{
    tft.fillScreen(TFT_WHITE);

    String val = String(getValue(menu, value_index));
    val.replace("!+", "(high)");
    val.replace("!-", "(low)");

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        val.c_str(), TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_TIPS_START_X, SCREEN_X, SUB_PAGE_TIPS_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO CONFIRM", TFT_BLACK, TFT_WHITE);
}

void Display::displayBLEConfirm()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_joystick);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "START BLE", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "GAMEPAD", TFT_BLACK, TFT_WHITE);
}

void Display::displayBLEStatus()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_joystick);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "BLE", TFT_BLACK, TFT_WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "GAMEPAD", TFT_BLACK, TFT_WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "RUNNING", TFT_BLACK, TFT_WHITE);
}

void Display::displayWiFiConfirm()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_wifimode);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "ENTER WIFI", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "UPDATE MODE", TFT_BLACK, TFT_WHITE);
}

void Display::displayWiFiStatus()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_wifimode);
    if (wifiMode == WIFI_STA) {
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            "open http://", TFT_BLACK, TFT_WHITE);

        String host_msg = String(wifi_hostname) + ".local";
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            host_msg, TFT_BLACK, TFT_WHITE);

        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            "by browser", TFT_BLACK, TFT_WHITE);
    }
    else
    {
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_ssid, TFT_BLACK, TFT_WHITE);

        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_password, TFT_BLACK, TFT_WHITE);

        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_address, TFT_BLACK, TFT_WHITE);
    }
}

void Display::displayBindConfirm()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_bind);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "SEND BIND", TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "REQUEST", TFT_BLACK, TFT_WHITE);
}

void Display::displayBindStatus()
{
    tft.fillScreen(TFT_WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "BINDING", TFT_BLACK, TFT_WHITE);
}

void Display::displayRunning()
{
    tft.fillScreen(TFT_WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "RUNNING", TFT_BLACK, TFT_WHITE);
}

#endif