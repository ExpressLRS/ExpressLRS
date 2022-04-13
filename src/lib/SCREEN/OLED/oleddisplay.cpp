#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) // This code will not be used if the hardware does not have a OLED display. Maybe a better way to blacklist it in platformio.ini?

#include <U8g2lib.h> // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO

#include "oleddisplay.h"

#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI
#include "options.h"
#include "logging.h"

#if defined(PLATFORM_ESP32)
#include "WiFi.h"
extern WiFiMode_t wifiMode;
#endif

// OLED specific header files.
U8G2 *u8g2;

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
    u8g2->clearBuffer();
    u8g2->drawXBMP(x, y, size, size, image);
    u8g2->sendBuffer();
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
        u8g2->clearBuffer();
#ifndef TARGET_TX_GHOST_LITE
        u8g2->drawXBMP((26 + i), 16, 32, 32, ghost);
        u8g2->drawXBMP((-31 + (i * 4)), 16, 32, 32, elrs32);
#else
        u8g2->drawXBMP((26 + i), 0, 32, 32, ghost);
        u8g2->drawXBMP((-31 + (i * 4)), 0, 32, 32, elrs32);
#endif
        u8g2->sendBuffer();
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

void OLEDDisplay::init()
{
    if (OPT_USE_OLED_SPI_SMALL)
        u8g2 = new U8G2_SSD1306_128X32_UNIVISION_F_4W_SW_SPI(OPT_OLED_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
    else if (OPT_USE_OLED_SPI)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI(OPT_OLED_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
    else if (OPT_USE_OLED_I2C)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(OPT_OLED_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_OLED_RST, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_SDA);

    u8g2->begin();
    u8g2->clearBuffer();
}

void OLEDDisplay::doScreenBackLight(screen_backlight_t state)
{
    #ifdef GPIO_PIN_OLED_BL
    if (GPIO_PIN_OLED_BL != UNDEF_PIN)
    {
        digitalWrite(GPIO_PIN_OLED_BL, state);
    }
    #endif
}

void OLEDDisplay::printScreenshot()
{
    u8g2->writeBufferXBM(*LoggingBackpack);
}

void OLEDDisplay::displaySplashScreen()
{
    u8g2->clearBuffer();
#ifdef TARGET_TX_GHOST
    ghostChase();
#else
#ifdef USE_OLED_SPI_SMALL
    if (OPT_USE_OLED_SPI_SMALL)
        u8g2->drawXBM(48, 0, 32, 32, elrs32);
    else
#endif
        u8g2->drawXBM(32, 0, 64, 64, elrs64);
#endif
    u8g2->sendBuffer();
}

void OLEDDisplay::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    u8g2->clearBuffer();
    String power = getValue(STATE_POWER, dynamic ? running_power_index : power_index);
    if (dynamic)
    {
        power += " *";
    }

    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->setFont(u8g2_font_t0_15_mr);
        u8g2->drawStr(0, 15, getValue(STATE_PACKET, rate_index));
        u8g2->drawStr(70, 15, getValue(STATE_TELEMETRY, ratio_index));
        u8g2->drawStr(0, 32, power.c_str());
        u8g2->drawStr(70, 32, version);
    }
    else
    {
        u8g2->setFont(u8g2_font_t0_15_mr);
        u8g2->drawStr(0, 13, message_string[message_index]);
        u8g2->drawStr(0, 45, getValue(STATE_PACKET, rate_index));
        u8g2->drawStr(70, 45, getValue(STATE_TELEMETRY, ratio_index));
        u8g2->drawStr(0, 60, power.c_str());
        u8g2->setFont(u8g2_font_profont10_mr);
        u8g2->drawStr(70, 56, "TLM");
        u8g2->drawStr(0, 27, "Ver: ");
        u8g2->drawStr(38, 27, version);
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayMainMenu(menu_item_t menu)
{
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, main_menu_strings[menu][0]);
        u8g2->drawStr(0,32, main_menu_strings[menu][1]);
    }
    else
    {
        u8g2->drawStr(0,20, main_menu_strings[menu][0]);
        u8g2->drawStr(0,50, main_menu_strings[menu][1]);
    }
    helperDrawImage(menu);
    u8g2->sendBuffer();
}

void OLEDDisplay::displayValue(menu_item_t menu, uint8_t value_index)
{
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_9x15_t_symbols);
    String val = String(getValue(menu, value_index));
    val.replace("!+", "\u2191");
    val.replace("!-", "\u2193");
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, val.c_str());
        u8g2->setFont(u8g2_font_profont10_mr);
        u8g2->drawStr(0,60, "PRESS TO CONFIRM");
    }
    else
    {
        u8g2->drawUTF8(0,20, val.c_str());
        u8g2->setFont(u8g2_font_profont10_mr);
        u8g2->drawStr(0,44, "PRESS TO");
        u8g2->drawStr(0,56, "CONFIRM");
    }
    helperDrawImage(menu);
    u8g2->sendBuffer();
}

void OLEDDisplay::displayBLEConfirm()
{
    // TODO: Put wifi image?
    u8g2->clearBuffer();

    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "PRESS TO");
        u8g2->drawStr(70,15, "START BLUETOOTH");
        u8g2->drawStr(0,32, "JOYSTICK");
    }
    else
    {
        u8g2->drawStr(0,29, "PRESS TO START");
        u8g2->drawStr(0,59, "BLE JOYSTICK");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayBLEStatus()
{
    u8g2->clearBuffer();

    // TODO: Add a fancy joystick symbol like the cool TFT peeps

    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "BLUETOOTH");
        u8g2->drawStr(70,15, "GAMEPAD");
        u8g2->drawStr(0,32, "RUNNING");
    }
    else
    {
        u8g2->drawStr(0,13, "BLUETOOTH");
        u8g2->drawStr(0,33, "GAMEPAD");
        u8g2->drawStr(0,63, "RUNNING");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayWiFiConfirm()
{
    // TODO: Put wifi image?
    u8g2->clearBuffer();

    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "PRESS TO");
        u8g2->drawStr(70,15, "ENTER WIFI");
        u8g2->drawStr(0,32, "UPDATE");
    }
    else
    {
        u8g2->drawStr(0,29, "PRESS TO ENTER");
        u8g2->drawStr(0,59, "WIFI UPDATE");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayWiFiStatus()
{
    u8g2->clearBuffer();
#if defined(PLATFORM_ESP32)
    // TODO: Add a fancy wifi symbol like the cool TFT peeps

    u8g2->setFont(u8g2_font_t0_17_mr);
    if (wifiMode == WIFI_STA) {
        if (OPT_USE_OLED_SPI_SMALL)
        {
            u8g2->drawStr(0,15, "open http://");
            u8g2->drawStr(70,15, (String(wifi_hostname)+".local").c_str());
            u8g2->drawStr(0,32, "by browser");
        }
        else
        {
            u8g2->drawStr(0,13, "open http://");
            u8g2->drawStr(0,33, (String(wifi_hostname)+".local").c_str());
            u8g2->drawStr(0,63, "by browser");
        }
    }
    else
    {
        if (OPT_USE_OLED_SPI_SMALL)
        {
            u8g2->drawStr(0,15, wifi_ap_ssid);
            u8g2->drawStr(70,15, wifi_ap_password);
            u8g2->drawStr(0,32, wifi_ap_address);
        }
        else
        {
            u8g2->drawStr(0,13, wifi_ap_ssid);
            u8g2->drawStr(0,33, wifi_ap_password);
            u8g2->drawStr(0,63, wifi_ap_address);
        }
    }
#endif
    u8g2->sendBuffer();
}

void OLEDDisplay::displayBindConfirm()
{
    // TODO: Put bind image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "PRESS TO");
        u8g2->drawStr(70,15 , "SEND BIND");
        u8g2->drawStr(0,32, "REQUEST");
    }
    else
    {
        u8g2->drawStr(0,29, "PRESS TO SEND");
        u8g2->drawStr(0,59, "BIND REQUEST");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayBindStatus()
{
    // TODO: Put bind image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "BINDING");
    }
    else
    {
        u8g2->drawStr(0,29, "BINDING");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayRunning()
{
    // TODO: Put wifi image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_USE_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0,15, "RUNNING");
    }
    else
    {
        u8g2->drawStr(0,29, "RUNNING");
    }
    u8g2->sendBuffer();
}

// helpers
static void helperDrawImage(menu_item_t menu)
{
    if (OPT_USE_OLED_SPI_SMALL)
   {

        // Adjust these to move them around on the screen
        int x_pos = 65;
        int y_pos = 5;

        switch(menu){
            case STATE_PACKET:
                u8g2->drawXBM(x_pos, y_pos, 32, 22, rate_img32);
                break;
            case STATE_POWER:
            case STATE_POWER_MAX:
            case STATE_POWER_DYNAMIC:
                u8g2->drawXBM(x_pos, y_pos, 25, 25, power_img32);
                break;
            case STATE_TELEMETRY:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, ratio_img32);
                break;
            case STATE_POWERSAVE:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, powersaving_img32);
                break;
            case STATE_SMARTFAN:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, fan_img32);
                break;
            case STATE_JOYSTICK:
                u8g2->drawXBM(x_pos, y_pos-5, 32, 32, joystick_img32);
                break;
            case STATE_VTX:
            case STATE_VTX_BAND:
            case STATE_VTX_CHANNEL:
            case STATE_VTX_POWER:
            case STATE_VTX_PITMODE:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, vtx_img32);
                break;
            case STATE_WIFI:
                u8g2->drawXBM(x_pos, y_pos, 24, 22, wifi_img32);
                break;
            case STATE_BIND:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, bind_img32);
                break;

            case STATE_WIFI_TX:
                u8g2->drawXBM(x_pos, y_pos, 24, 22, wifi_img32);
                break;
            case STATE_WIFI_RX:
                u8g2->drawXBM(x_pos, y_pos-5, 32, 32, rxwifi_img32);
                break;
            case STATE_WIFI_BACKPACK:
                u8g2->drawXBM(x_pos, y_pos-5, 32, 32, backpack_img32);
                break;
            case STATE_WIFI_VRX:
                u8g2->drawXBM(x_pos, y_pos-5, 32, 32, vrxwifi_img32);
                break;

            default:
                break;
        }
    }
    else
    {
        // Adjust these to move them around on the screen
        int x_pos = 65;
        int y_pos = 5;

        switch(menu){
            case STATE_PACKET:
                u8g2->drawXBM(x_pos, y_pos, 64, 44, rate_img64);
                break;
            case STATE_POWER:
            case STATE_POWER_MAX:
            case STATE_POWER_DYNAMIC:
                u8g2->drawXBM(x_pos, y_pos, 50, 50, power_img64);
                break;
            case STATE_TELEMETRY:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, ratio_img64);
                break;
            case STATE_POWERSAVE:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, powersaving_img64);
                break;
            case STATE_SMARTFAN:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, fan_img64);
                break;
            case STATE_JOYSTICK:
                u8g2->drawXBM(x_pos, y_pos, 64, 64-5, joystick_img64);
                break;
            case STATE_VTX:
            case STATE_VTX_BAND:
            case STATE_VTX_CHANNEL:
            case STATE_VTX_POWER:
            case STATE_VTX_PITMODE:
            case STATE_VTX_SEND:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, vtx_img64);
                break;
            case STATE_WIFI:
                u8g2->drawXBM(x_pos, y_pos, 48, 44, wifi_img64);
                break;
            case STATE_BIND:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, bind_img64);
                break;

            case STATE_WIFI_TX:
                u8g2->drawXBM(x_pos, y_pos, 48, 44, wifi_img64);
                break;
            case STATE_WIFI_RX:
                u8g2->drawXBM(x_pos, y_pos-5, 64, 64, rxwifi_img64);
                break;
            case STATE_WIFI_BACKPACK:
                u8g2->drawXBM(x_pos, y_pos-5, 64, 64, backpack_img64);
                break;
            case STATE_WIFI_VRX:
                u8g2->drawXBM(x_pos, y_pos-5, 64, 64, vrxwifi_img64);
                break;

            default:
                break;
        }
    }
}

#endif