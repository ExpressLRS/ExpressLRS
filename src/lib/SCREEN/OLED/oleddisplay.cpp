#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) // This code will not be used if the hardware does not have a OLED display. Maybe a better way to blacklist it in platformio.ini?

#include <U8g2lib.h> // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO

#include "display.h"

#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI
#include "options.h"
#include "logging.h"

// OLED specific header files.

#ifdef OLED_REVERSED
#define OLED_ROTATION U8G2_R2
#else
#define OLED_ROTATION U8G2_R0
#endif

#ifdef USE_OLED_SPI_SMALL
U8G2_SSD1306_128X32_UNIVISION_F_4W_SW_SPI u8g2(OLED_ROTATION, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
#endif

#ifdef USE_OLED_SPI
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(OLED_ROTATION, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
#endif

#ifdef USE_OLED_I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(OLED_ROTATION, GPIO_PIN_OLED_RST, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_SDA);
#endif

#ifdef TARGET_TX_GHOST
/**
 * helper function is used to draw xbmp on the OLED.
 * x = x position of the image
 * y = y position of the image
 * size = demensions of the box size x size, this only works for square images 1:1
 * image = XBM character string
 */
#ifndef TARGET_TX_GHOST_LITE
static void helper(int x, int y, int size, const unsigned char *image)
{
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, y, size, size, image);
    u8g2.sendBuffer();
}
#endif

/**
 *  ghostChase will only be called for ghost TX hardware.
 */
static void ghostChase()
{
    // Using i < 16 and (i*4) to get 64 total pixels. Change to i < 32 (i*2) to slow animation.
    for (int i = 0; i < 20; i++)
    {
        u8g2.clearBuffer();
#ifndef TARGET_TX_GHOST_LITE
        u8g2.drawXBMP((26 + i), 16, 32, 32, ghost);
        u8g2.drawXBMP((-31 + (i * 4)), 16, 32, 32, elrs32);
#else
        u8g2.drawXBMP((26 + i), 0, 32, 32, ghost);
        u8g2.drawXBMP((-31 + (i * 4)), 0, 32, 32, elrs32);
#endif
        u8g2.sendBuffer();
    }
/**
 *  Animation for the ghost logo expanding in the center of the screen.
 *  helper function just draw's the XBM strings.
 */
#ifndef TARGET_TX_GHOST_LITE
    helper(38, 12, 40, elrs40);
    helper(36, 8, 48, elrs48);
    helper(34, 4, 56, elrs56);
    helper(32, 0, 64, elrs64);
#endif
}
#endif

static void helperDrawImage(menu_item_t menu);

void Display::init()
{
    u8g2.begin();
    u8g2.clearBuffer();
}

void Display::doScreenBackLight(screen_backlight_t state)
{
    #ifdef GPIO_PIN_OLED_BL
    digitalWrite(GPIO_PIN_OLED_BL, state);
    #endif
}

void Display::displaySplashScreen()
{
    u8g2.clearBuffer();
#ifdef TARGET_TX_GHOST
    ghostChase();
#else
#ifdef USE_OLED_SPI_SMALL
    u8g2.drawXBM(48, 0, 32, 32, elrs32);
#else
    u8g2.drawXBM(32, 0, 64, 64, elrs64);
#endif
#endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("splash_screen");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    u8g2.clearBuffer();
    String power = value_sets[MENU_POWER].values[power_index];
    if (dynamic)
    {
        power += " *";
    }

#ifdef USE_OLED_SPI_SMALL
    u8g2.setFont(u8g2_font_t0_15_mr);
    u8g2.drawStr(0, 15, value_sets[MENU_PACKET].values[rate_index]);
    u8g2.drawStr(70, 15, value_sets[MENU_TELEMETRY].values[ratio_index]);
    u8g2.drawStr(0, 32, power.c_str());
    u8g2.drawStr(70, 32, version);
#else
    u8g2.setFont(u8g2_font_t0_15_mr);
    u8g2.drawStr(0, 13, message_string[message_index]);
    u8g2.drawStr(0, 45, value_sets[MENU_PACKET].values[rate_index]);
    u8g2.drawStr(70, 45, value_sets[MENU_TELEMETRY].values[ratio_index]);
    u8g2.drawStr(0, 60, power.c_str());
    u8g2.setFont(u8g2_font_profont10_mr);
    u8g2.drawStr(70, 56, "TLM");
    u8g2.drawStr(0, 27, "Ver: ");
    u8g2.drawStr(38, 27, version);
#endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("idle_screen");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayMainMenu(menu_item_t menu)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_17_mr);
    #ifdef USE_OLED_SPI_SMALL
        u8g2.drawStr(0,15, main_menu_line_1[menu]);
        u8g2.drawStr(0,32, main_menu_line_2[menu]);
    #else
        u8g2.drawStr(0,20, main_menu_line_1[menu]);
        u8g2.drawStr(0,50, main_menu_line_2[menu]);
    #endif
    helperDrawImage(menu);
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("main_menu_%d", menu);
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayValue(menu_item_t menu, uint8_t value_index)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_16_mr);
    #ifdef USE_OLED_SPI_SMALL
        u8g2.drawStr(0,15, value_sets[menu].values[value_index]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.drawStr(0,20, value_sets[menu].values[value_index]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,44, "PRESS TO");
        u8g2.drawStr(0,56, "CONFIRM");
    #endif
    helperDrawImage(menu);
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("menu_value_%d_%d", menu, value_index);
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayWiFiConfirm()
{
    // TODO: Put wifi image?
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_t0_17_mr);
    #ifdef USE_OLED_SPI_SMALL
        u8g2.drawStr(0,15, "PRESS TO");
        u8g2.drawStr(70,15, "ENTER WIFI");
        u8g2.drawStr(0,32, "UPDATE");
    #else
        u8g2.drawStr(0,29, "PRESS TO ENTER");
        u8g2.drawStr(0,59, "WIFI UPDATE");
    #endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("wifi_confirm");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayWiFiStatus()
{
    u8g2.clearBuffer();

    // TODO: Add a fancy wifi symbol like the cool TFT peeps

    u8g2.setFont(u8g2_font_t0_17_mr);
    #if defined(HOME_WIFI_SSID) && defined(HOME_WIFI_PASSWORD)
        #ifdef USE_OLED_SPI_SMALL
            u8g2.drawStr(0,15, "open http://");
            u8g2.drawStr(70,15, (String(wifi_hostname)+".local").c_str());
            u8g2.drawStr(0,32, "by browser");
        #else
            u8g2.drawStr(0,13, "open http://");
            u8g2.drawStr(0,33, (String(wifi_hostname)+".local").c_str());
            u8g2.drawStr(0,63, "by browser");
        #endif
    #else
        #ifdef USE_OLED_SPI_SMALL
            u8g2.drawStr(0,15, wifi_ap_ssid);
            u8g2.drawStr(70,15, wifi_ap_password);
            u8g2.drawStr(0,32, wifi_ap_address);
        #else
            u8g2.drawStr(0,13, wifi_ap_ssid);
            u8g2.drawStr(0,33, wifi_ap_password);
            u8g2.drawStr(0,63, wifi_ap_address);
        #endif
    #endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("wifi_status");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayBindConfirm()
{
    // TODO: Put bind image?
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_17_mr);
    #ifdef USE_OLED_SPI_SMALL
        u8g2.drawStr(0,15, "PRESS TO");
        u8g2.drawStr(70,15 , "SEND BIND");
        u8g2.drawStr(0,32, "REQUEST");
    #else
        u8g2.drawStr(0,29, "PRESS TO SEND");
        u8g2.drawStr(0,59, "BIND REQUEST");
    #endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("bind_confirm");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

void Display::displayBindStatus()
{
    // TODO: Put bind image?
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_17_mr);
    #ifdef USE_OLED_SPI_SMALL
        u8g2.drawStr(0,15, "BINDING");
    #else
        u8g2.drawStr(0,29, "BINDING");
    #endif
    u8g2.sendBuffer();
    #ifdef DEBUG_SCREENSHOT
    DBGLN("bind_status");
    u8g2.writeBufferXBM(*LoggingBackpack);
    #endif
}

// helpers

#ifdef USE_OLED_SPI_SMALL
static void helperDrawImage(menu_item_t menu)
{
    // Adjust these to move them around on the screen
    int x_pos = 65;
    int y_pos = 5;

    switch(menu){
        case 0:
            u8g2.drawXBM(x_pos, y_pos, 32, 22, rate_img32);
            break;
        case 1:
            u8g2.drawXBM(x_pos, y_pos, 25, 25, power_img32);
            break;
        case 2:
            u8g2.drawXBM(x_pos, y_pos, 32, 32, ratio_img32);
            break;
        case 3:
            u8g2.drawXBM(x_pos, y_pos, 32, 32, powersaving_img32);
            break;
        case 4:
            u8g2.drawXBM(x_pos, y_pos, 32, 32, fan_img32);
            break;
        case 5:
            u8g2.drawXBM(x_pos, y_pos, 32, 32, bind_img32);
            break;
        case 6:
            u8g2.drawXBM(x_pos, y_pos, 24, 22, wifi_img32);
            break;

    }
}
#else
static void helperDrawImage(menu_item_t menu)
{
    // Adjust these to move them around on the screen
    int x_pos = 65;
    int y_pos = 5;

    switch(menu){
        case MENU_PACKET:
            u8g2.drawXBM(x_pos, y_pos, 64, 44, rate_img64);
            break;
        case MENU_POWER:
            u8g2.drawXBM(x_pos, y_pos, 50, 50, power_img64);
            break;
        case MENU_TELEMETRY:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, ratio_img64);
            break;
        case MENU_POWERSAVE:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, powersaving_img64);
            break;
        case MENU_SMARTFAN:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, fan_img64);
            break;
        case MENU_WIFI:
            u8g2.drawXBM(x_pos, y_pos, 48, 44, wifi_img64);
            break;
        case MENU_BIND:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, bind_img64);
            break;
    }
}
#endif

#endif