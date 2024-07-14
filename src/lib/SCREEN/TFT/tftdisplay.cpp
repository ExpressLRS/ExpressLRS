#ifdef HAS_TFT_SCREEN

#include <Arduino_GFX_Library.h>
#include "Pragma_Sans36pt7b.h"
#include "Pragma_Sans37pt7b.h"
#include "Pragma_Sans314pt7b.h"

#include "tftdisplay.h"

#include "logos.h"
#include "options.h"
#include "logging.h"
#include "common.h"

#include "WiFi.h"
extern WiFiMode_t wifiMode;

const uint16_t *main_menu_icons[] = {
    elrs_rate,
    elrs_switch,
    elrs_antenna,
    elrs_power,
    elrs_ratio,
    elrs_motion,
    elrs_fan,
    elrs_joystick,
    elrs_bind,
    elrs_wifimode,
    elrs_vtx,

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
    0xF501, // MSG_MISMATCH      => #F0A30A (amber)
    0xF800  // MSG_ERROR         => #F0A30A (very red)
};

#define SCREEN_X    160
#define SCREEN_Y    80

#define SCREEN_SMALL_FONT_SIZE      8
#define SCREEN_SMALL_FONT           Pragma_Sans36pt7b

#define SCREEN_NORMAL_FONT_SIZE     16
#define SCREEN_NORMAL_FONT          Pragma_Sans37pt7b

#define SCREEN_LARGE_FONT_SIZE      26
#define SCREEN_LARGE_FONT           Pragma_Sans314pt7b

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

#define IDLE_PAGE_STAT_START_X  SCREEN_X/2 // is centered, so no GAP
#define IDLE_PAGE_STAT_Y_GAP    (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE * 3)/4

#define IDLE_PAGE_RATE_START_Y  IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_POWER_START_Y IDLE_PAGE_RATE_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_RATIO_START_Y IDLE_PAGE_POWER_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP

//MAIN PAGE Definition
#define MAIN_PAGE_ICON_START_X  SCREEN_CONTENT_GAP
#define MAIN_PAGE_ICON_START_Y  (SCREEN_Y -  SCREEN_LARGE_ICON_SIZE)/2

#define MAIN_PAGE_WORD_START_X  MAIN_PAGE_ICON_START_X + SCREEN_LARGE_ICON_SIZE
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

static Arduino_DataBus *bus;
static Arduino_GFX *gfx;

void TFTDisplay::init()
{
    if (GPIO_PIN_TFT_BL != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_TFT_BL, OUTPUT);
    }
    bus = new Arduino_ESP32SPI(GPIO_PIN_TFT_DC, GPIO_PIN_TFT_CS, GPIO_PIN_TFT_SCLK, GPIO_PIN_TFT_MOSI, GFX_NOT_DEFINED, HSPI);
    gfx = new Arduino_ST7735(bus, GPIO_PIN_TFT_RST, OPT_OLED_REVERSED ? 3 : 1 /* rotation */, true , 80, 160, 26, 1, 26, 1);

    gfx->begin();
    doScreenBackLight(SCREEN_BACKLIGHT_ON);
}

void TFTDisplay::doScreenBackLight(screen_backlight_t state)
{
    if (GPIO_PIN_TFT_BL != UNDEF_PIN)
    {
        digitalWrite(GPIO_PIN_TFT_BL, state);
    }
}

void TFTDisplay::printScreenshot()
{
    DBGLN("Unimplemented");
}

static void displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y,
                                            int font_size, const GFXfont& font, String font_string,
                                            uint16_t fgColor, uint16_t bgColor)
{
    gfx->fillRect(font_start_x, font_start_y, font_end_x - font_start_x, font_size, bgColor);
    gfx->setFont(&font);

    int16_t x, y;
    uint16_t w, h;
    gfx->getTextBounds(font_string, font_start_x, font_start_y, &x, &y, &w, &h);
    int start_pos = font_start_x + (font_end_x - font_start_x - w)/2;

    gfx->setCursor(start_pos, font_start_y + h);
    gfx->setTextColor(fgColor, bgColor);
    gfx->print(font_string);
}


void TFTDisplay::displaySplashScreen()
{
    gfx->fillScreen(WHITE);

    size_t sz = INIT_PAGE_LOGO_X * INIT_PAGE_LOGO_Y;
    uint16_t image[sz];
    if (spi_flash_read(logo_image, image, sz * 2) == ESP_OK)
    {
        gfx->draw16bitRGBBitmap(0, 0, image, INIT_PAGE_LOGO_X, INIT_PAGE_LOGO_Y);
    }

    gfx->fillRect(SCREEN_FONT_GAP, INIT_PAGE_FONT_START_Y - INIT_PAGE_FONT_PADDING,
                    SCREEN_X - SCREEN_FONT_GAP*2, SCREEN_NORMAL_FONT_SIZE + INIT_PAGE_FONT_PADDING*2, BLACK);

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%s  ELRS-%.6s", HARDWARE_VERSION, version);
    displayFontCenter(INIT_PAGE_FONT_START_X, SCREEN_X - INIT_PAGE_FONT_START_X, INIT_PAGE_FONT_START_Y,
                        SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        String(buffer), WHITE, BLACK);
}

void TFTDisplay::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    if (changed == CHANGED_ALL)
    {
        // Everything has changed! So clear the right side
        gfx->fillRect(SCREEN_X/2, 0, SCREEN_X/2, SCREEN_Y, WHITE);
    }

    if (changed & CHANGED_TEMP)
    {
        // Left side logo, version, and temp
        gfx->fillRect(0, 0, SCREEN_X/2, SCREEN_Y, elrs_banner_bgColor[message_index]);
        gfx->drawBitmap(IDLE_PAGE_START_X, IDLE_PAGE_START_Y, elrs_banner_bmp, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE,
                        WHITE);

        // Update the temperature
        char buffer[20];
        // \367 = (char)247 = degree symbol
        snprintf(buffer, sizeof(buffer), "%.6s %02d\367C", version, temperature);
        displayFontCenter(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2,
                            SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT,
                            String(buffer), WHITE, elrs_banner_bgColor[message_index]);
    }

    // The Radio Params right half of the screen
    uint16_t text_color = (message_index == MSG_ARMED) ? DARKGREY : BLACK;

    if (connectionState == radioFailed)
    {
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
            "BAD", BLACK, WHITE);
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
            "RADIO", BLACK, WHITE);
    }
    else if (connectionState == noCrossfire)
    {
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
            "NO", BLACK, WHITE);
        displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
            "HANDSET", BLACK, WHITE);
    }
    else
    {
        if (changed & CHANGED_RATE)
        {
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATE_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                                getValue(STATE_PACKET, rate_index), text_color, WHITE);
        }

        if (changed & CHANGED_POWER)
        {
            String power = getValue(STATE_POWER, running_power_index);
            if (dynamic || power_index != running_power_index)
            {
                power += " *";
            }
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_POWER_START_Y, SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                                power, text_color, WHITE);
        }

        if (changed & CHANGED_TELEMETRY)
        {
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATIO_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                                getValue(STATE_TELEMETRY_CURR, ratio_index), text_color, WHITE);
        }
    }

}

void TFTDisplay::displayMainMenu(menu_item_t menu)
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(MAIN_PAGE_ICON_START_X, MAIN_PAGE_ICON_START_Y, main_menu_icons[menu], SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
        main_menu_strings[menu][0], BLACK, WHITE);
    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
        main_menu_strings[menu][1], BLACK, WHITE);
}

void TFTDisplay::displayValue(menu_item_t menu, uint8_t value_index)
{
    gfx->fillScreen(WHITE);

    String val = String(getValue(menu, value_index));
    val.replace("!+", "\xA0");
    val.replace("!-", "\xA1");

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        val.c_str(), BLACK, WHITE);
    displayFontCenter(SUB_PAGE_TIPS_START_X, SCREEN_X, SUB_PAGE_TIPS_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO CONFIRM", BLACK, WHITE);
}

void TFTDisplay::displayBLEConfirm()
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, elrs_joystick, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "START BLE", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "GAMEPAD", BLACK, WHITE);
}

void TFTDisplay::displayBLEStatus()
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, elrs_joystick, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "BLE", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "GAMEPAD", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "RUNNING", BLACK, WHITE);
}

void TFTDisplay::displayWiFiConfirm()
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, elrs_wifimode, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "ENTER WIFI", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "UPDATE MODE", BLACK, WHITE);
}

void TFTDisplay::displayWiFiStatus()
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, elrs_wifimode, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    if (wifiMode == WIFI_STA) {
        String host_msg = String(wifi_hostname) + ".local";
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            "open http://", BLACK, WHITE);
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            host_msg, BLACK, WHITE);
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            "by browser", BLACK, WHITE);
    }
    else
    {
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_ssid, BLACK, WHITE);
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_password, BLACK, WHITE);
        displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                            wifi_ap_address, BLACK, WHITE);
    }
}

void TFTDisplay::displayBindConfirm()
{
    gfx->fillScreen(WHITE);

    gfx->draw16bitRGBBitmap(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, elrs_bind, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "SEND BIND", BLACK, WHITE);
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "REQUEST", BLACK, WHITE);
}

void TFTDisplay::displayBindStatus()
{
    gfx->fillScreen(WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "BINDING...", BLACK, WHITE);
}

void TFTDisplay::displayRunning()
{
    gfx->fillScreen(WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "RUNNING...", BLACK, WHITE);
}

void TFTDisplay::displaySending()
{
    gfx->fillScreen(WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "SENDING...", BLACK, WHITE);
}

void TFTDisplay::displayLinkstats()
{
    gfx->fillScreen(WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "linkstats...", BLACK, WHITE);
}

#endif
