#if defined(PLATFORM_ESP32)

#include <U8g2lib.h> // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO

#include "oleddisplay.h"

#include "CRSFRouter.h"
#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI
#include "common.h"
#include "logging.h"
#include "options.h"

#include "WiFi.h"
extern WiFiMode_t wifiMode;

// OLED specific header files.
U8G2 *u8g2;

static void helperDrawImage(menu_item_t menu);
static void drawCentered(u8g2_int_t y, const char *str)
{
    u8g2_int_t x = (u8g2->getDisplayWidth() - u8g2->getStrWidth(str)) / 2;
    u8g2->drawStr(x, y, str);
}

void OLEDDisplay::init()
{
    if (OPT_HAS_OLED_SPI_SMALL)
        u8g2 = new U8G2_SSD1306_128X32_UNIVISION_F_4W_SW_SPI(OPT_SCREEN_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_MOSI, GPIO_PIN_SCREEN_CS, GPIO_PIN_SCREEN_DC, GPIO_PIN_SCREEN_RST);
    else if (OPT_HAS_OLED_SPI)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI(OPT_SCREEN_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_MOSI, GPIO_PIN_SCREEN_CS, GPIO_PIN_SCREEN_DC, GPIO_PIN_SCREEN_RST);
    else if (OPT_HAS_OLED_I2C)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(OPT_SCREEN_REVERSED ? U8G2_R2 : U8G2_R0, GPIO_PIN_SCREEN_RST, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_SDA);

    u8g2->begin();
    u8g2->clearBuffer();
}

void OLEDDisplay::doScreenBackLight(screen_backlight_t state)
{
    if (GPIO_PIN_SCREEN_BL != UNDEF_PIN)
    {
        digitalWrite(GPIO_PIN_SCREEN_BL, state);
    }
    if (state == SCREEN_BACKLIGHT_OFF)
    {
        u8g2->clearDisplay();
        u8g2->setPowerSave(true);
    }
    else
    {
        u8g2->setPowerSave(false);
    }
}

void OLEDDisplay::printScreenshot()
{
    u8g2->writeBufferXBM(*TxBackpack);
}

void OLEDDisplay::displaySplashScreen()
{
    u8g2->clearBuffer();
    if (OPT_HAS_OLED_SPI_SMALL)
    {
        auto constexpr sz = 128 * 32 / 8;
        uint8_t image[sz];
        if (spi_flash_read(logo_image, image, sz) == ESP_OK)
        {
            u8g2->drawXBM(0, 0, 128, 32, image);
        }
    }
    else
    {
        auto constexpr sz = 128 * 64 / 8;
        uint8_t image[sz];
        if (spi_flash_read(logo_image, image, sz) == ESP_OK)
        {
            u8g2->drawXBM(0, 0, 128, 64, image);
        }

        char buffer[50];
        snprintf(buffer, sizeof(buffer), "ELRS-%.6s", version);
        u8g2->setFont(u8g2_font_profont10_mr);
        drawCentered(60, buffer);
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    u8g2->clearBuffer();
    String power = getValue(STATE_POWER, running_power_index);
    if (dynamic || power_index != running_power_index)
    {
        power += " *";
    }

    u8g2->setFont(u8g2_font_t0_15_mr);
    if (connectionState == radioFailed)
    {
        drawCentered(15, "BAD");
        drawCentered(32, "RADIO");
    }
    else if (connectionState == noCrossfire)
    {
        drawCentered(15, "NO");
        drawCentered(32, "HANDSET");
    }
    else if (OPT_HAS_OLED_SPI_SMALL)
    {
        u8g2->drawStr(0, 15, getValue(STATE_PACKET, rate_index));
        u8g2->drawStr(70, 15, getValue(STATE_TELEMETRY_CURR, ratio_index));
        u8g2->drawStr(0, 32, power.c_str());
        u8g2->drawStr(70, 32, version);
    }
    else
    {
        u8g2->drawStr(0, 13, message_string[message_index]);
        u8g2->drawStr(0, 45, getValue(STATE_PACKET, rate_index));
        u8g2->drawStr(70, 45, getValue(STATE_TELEMETRY_CURR, ratio_index));
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
    if (OPT_HAS_OLED_SPI_SMALL)
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
    if (OPT_HAS_OLED_SPI_SMALL)
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
    if (OPT_HAS_OLED_SPI_SMALL)
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
    if (OPT_HAS_OLED_SPI_SMALL)
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
    if (OPT_HAS_OLED_SPI_SMALL)
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
    // TODO: Add a fancy wifi symbol like the cool TFT peeps

    u8g2->setFont(u8g2_font_t0_17_mr);
    if (wifiMode == WIFI_STA) {
        if (OPT_HAS_OLED_SPI_SMALL)
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
        if (OPT_HAS_OLED_SPI_SMALL)
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
    u8g2->sendBuffer();
}

void OLEDDisplay::displayBindConfirm()
{
    // TODO: Put bind image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_HAS_OLED_SPI_SMALL)
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
    if (OPT_HAS_OLED_SPI_SMALL)
    {
        drawCentered(15, "BINDING...");
    }
    else
    {
        drawCentered(29, "BINDING...");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayRunning()
{
    // TODO: Put wifi image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_HAS_OLED_SPI_SMALL)
    {
        drawCentered(15, "RUNNING...");
    }
    else
    {
        drawCentered(29, "RUNNING...");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displaySending()
{
    // TODO: Put wifi image?
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_t0_17_mr);
    if (OPT_HAS_OLED_SPI_SMALL)
    {
        drawCentered(15, "SENDING...");
    }
    else
    {
        drawCentered(29, "SENDING...");
    }
    u8g2->sendBuffer();
}

void OLEDDisplay::displayLinkstats()
{
    constexpr int16_t LINKSTATS_COL_FIRST   = 0;
    constexpr int16_t LINKSTATS_COL_SECOND  = 32;
    constexpr int16_t LINKSTATS_COL_THIRD   = 85;

    constexpr int16_t LINKSTATS_ROW_FIRST   = 10;
    constexpr int16_t LINKSTATS_ROW_SECOND  = 20;
    constexpr int16_t LINKSTATS_ROW_THIRD   = 30;
    constexpr int16_t LINKSTATS_ROW_FOURTH  = 40;
    constexpr int16_t LINKSTATS_ROW_FIFTH   = 50;

    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_profont10_mr);

    u8g2->drawStr(LINKSTATS_COL_FIRST, LINKSTATS_ROW_SECOND, "LQ");
    u8g2->drawStr(LINKSTATS_COL_FIRST, LINKSTATS_ROW_THIRD, "RSSI");
    u8g2->drawStr(LINKSTATS_COL_FIRST, LINKSTATS_ROW_FOURTH, "SNR");
    u8g2->drawStr(LINKSTATS_COL_FIRST, LINKSTATS_ROW_FIFTH, "Ant");

    u8g2->drawStr(LINKSTATS_COL_SECOND, LINKSTATS_ROW_FIRST, "Uplink");
    u8g2->setCursor(LINKSTATS_COL_SECOND, LINKSTATS_ROW_SECOND);
    u8g2->print(linkStats.uplink_Link_quality);
    u8g2->setCursor(LINKSTATS_COL_SECOND, LINKSTATS_ROW_THIRD);
    u8g2->print((int8_t)linkStats.uplink_RSSI_1);
    if (linkStats.uplink_RSSI_2 != 0)
    {
        u8g2->print('/');
        u8g2->print((int8_t)linkStats.uplink_RSSI_2);
    }

    u8g2->drawStr(LINKSTATS_COL_THIRD, LINKSTATS_ROW_FIRST, "Downlink");
    u8g2->setCursor(LINKSTATS_COL_THIRD, LINKSTATS_ROW_SECOND);
    u8g2->print(linkStats.downlink_Link_quality);
    u8g2->setCursor(LINKSTATS_COL_THIRD, LINKSTATS_ROW_THIRD);
    u8g2->print((int8_t)linkStats.downlink_RSSI_1);
    if (isDualRadio())
    {
        u8g2->print('/');
        u8g2->print((int8_t)linkStats.downlink_RSSI_2);
    }

    if (!OPT_HAS_OLED_SPI_SMALL)
    {
        u8g2->setCursor(LINKSTATS_COL_SECOND, LINKSTATS_ROW_FOURTH);
        u8g2->print((int8_t)linkStats.uplink_SNR);
        u8g2->setCursor(LINKSTATS_COL_THIRD, LINKSTATS_ROW_FOURTH);
        u8g2->print((int8_t)linkStats.downlink_SNR);
        u8g2->setCursor(LINKSTATS_COL_SECOND, LINKSTATS_ROW_FIFTH);
        u8g2->print(linkStats.active_antenna);
    }

    u8g2->sendBuffer();
}

// helpers
static void helperDrawImage(menu_item_t menu)
{
    if (OPT_HAS_OLED_SPI_SMALL)
    {

        // Adjust these to move them around on the screen
        int x_pos = 65;
        int y_pos = 5;

        switch(menu){
            case STATE_PACKET:
                u8g2->drawXBM(x_pos, y_pos, 32, 22, rate_img32);
                break;
            case STATE_SWITCH:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, switch_img32);
                break;
            case STATE_ANTENNA:
                u8g2->drawXBM(x_pos, y_pos, 32, 32, antenna_img32);
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
            case STATE_VTX_SEND:
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
            case STATE_SWITCH:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, switch_img64);
                break;
            case STATE_ANTENNA:
                u8g2->drawXBM(x_pos, y_pos, 64, 64, antenna_img64);
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
