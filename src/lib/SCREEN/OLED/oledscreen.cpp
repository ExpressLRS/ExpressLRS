/*
 * This file is part of the ExpressLRS distribution (https://github.com/ExpressLRS/ExpressLRS).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) // This code will not be used if the hardware does not have a OLED display. Maybe a better way to blacklist it in platformio.ini?
// Default header files for Express LRS
#include "targets.h"
#include "options.h"

// OLED specific header files.
#include "oledscreen.h"
#include <U8g2lib.h>    // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO
#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI

// Used for the the max power settings
#include "POWERMGNT.h"

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
static void helper(int x, int y, int size,  const unsigned char * image){
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, y, size, size, image);
    u8g2.sendBuffer();
}
#endif

/**
 *  ghostChase will only be called for ghost TX hardware.
 */
static void ghostChase(){
    // Using i < 16 and (i*4) to get 64 total pixels. Change to i < 32 (i*2) to slow animation.
    for(int i = 0; i < 20; i++){
        u8g2.clearBuffer();
        #ifndef TARGET_TX_GHOST_LITE
            u8g2.drawXBMP((26 + i), 16, 32, 32, ghost);
            u8g2.drawXBMP((-31 + (i*4)), 16, 32, 32, elrs32);
        #else
            u8g2.drawXBMP((26 + i), 0, 32, 32, ghost);
            u8g2.drawXBMP((-31 + (i*4)), 0, 32, 32, elrs32);
        #endif
        u8g2.sendBuffer();
    }
    /**
     *  Animation for the ghost logo expanding in the center of the screen.
     *  helper function just draw's the XBM strings.
     */
    #ifndef TARGET_TX_GHOST_LITE
        helper(38,12,40,elrs40);
        helper(36,8,48,elrs48);
        helper(34,4,56,elrs56);
        helper(32,0,64,elrs64);
    #endif
}
#endif

/**
 * Displays the ExpressLRS logo
 *
 * @param values none
 * @return void
 */
static void displayLogo()
{
    u8g2.begin();
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
}


static void displayFontCenter(const char * info)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont10_mr);
#ifdef USE_OLED_SPI_SMALL
    u8g2.drawStr(32, 32, info);
#else
    u8g2.drawStr(64, 64, info);
#endif
    u8g2.sendBuffer();
}

void OLEDScreen::init(bool reboot)
{
    u8g2.begin();

    displayLogo();


    current_screen_status = SCREEN_STATUS_INIT;

    current_rate_index = 0;
    current_power_index = 0;
    current_ratio_index = 0;

    current_page_index = PAGE_MAIN_MENU_INDEX;

    main_menu_page_index = MAIN_MENU_RATE_INDEX;

    u8g2.clearBuffer();
}

void helperDrawImage64(int menu)
{
    // Adjust these to move them around on the screen
    int x_pos = 65;
    int y_pos = 5;

    switch(menu){
        case 0:
            u8g2.drawXBM(x_pos, y_pos, 64, 44, rate_img64);
            break;
        case 1:
            u8g2.drawXBM(x_pos, y_pos, 50, 50, power_img64);
            break;
        case 2:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, ratio_img64);
            break;
        case 3:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, powersaving_img64);
            break;
        case 4:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, fan_img64);
            break;
        case 5:
            u8g2.drawXBM(x_pos, y_pos, 64, 64, bind_img64);
            break;
        case 6:
            u8g2.drawXBM(x_pos, y_pos, 48, 44, wifi_img64);
            break;
    }
}

void helperDrawImage32(int menu)
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

void OLEDScreen::displayMainScreen(){
    u8g2.clearBuffer();
    String power = power_string[current_power_index];
    if (current_dynamic)
    {
        power = String(power_string[last_power_index]) + " *";
    }

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_15_mr);
        u8g2.drawStr(0,15, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(70,15, &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,32, power.c_str());
        char buffer[7];
        strncpy(buffer, version, 6);
        buffer[6] = 0;
        u8g2.drawStr(70,32, buffer);
    #else
        #if (defined(TARGET_TX_EMAX_900)) || defined(TARGET_TX_EMAX_2400_V1)
            u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
            u8g2.setFontMode(1); 
            u8g2.drawStr(3, 10, "ExpressLRS");
            u8g2.drawStr(3, 22, "Pkt.Rate");
            u8g2.drawStr(80, 22, &(rate_string[current_rate_index])[0]);
            u8g2.drawStr(3, 34, "TLM Ratio");
            u8g2.drawStr(80, 34, &(ratio_string[current_ratio_index])[0]);
            u8g2.drawStr(3, 46, "Power");
            u8g2.drawStr(80, 46, power.c_str());
            u8g2.drawStr(3, 58, "RF Freq");
            #ifdef Regulatory_Domain_AU_433
                u8g2.drawStr(75, 58, "915 FCC");
            #elif defined Regulatory_Domain_AU_915
                u8g2.drawStr(75, 58, "EU_868");
            #elif defined Regulatory_Domain_EU_868
                u8g2.drawStr(75, 58, "866");
            #elif defined Regulatory_Domain_IN_866
                u8g2.drawStr(75, 58, "AU_915");
            #elif defined Regulatory_Domain_EU_433
                u8g2.drawStr(75, 58, "EU_433");
            #elif defined Regulatory_Domain_FCC_915
                u8g2.drawStr(75, 58, "FCC_915");
            #elif Regulatory_Domain_ISM_2400
                u8g2.drawStr(75, 58, "ISM_2400");
            #endif
        #else
            u8g2.setFont(u8g2_font_t0_15_mr);
            u8g2.drawStr(0,13, "ExpressLRS");
            u8g2.drawStr(0,45, &(rate_string[current_rate_index])[0]);
            u8g2.drawStr(70,45, &(ratio_string[current_ratio_index])[0]);
            u8g2.drawStr(0,60, power.c_str());
            u8g2.setFont(u8g2_font_profont10_mr);
            u8g2.drawStr(70,56, "TLM");
            u8g2.drawStr(0,27, "Ver: ");
            char buffer[7];
            strncpy(buffer, version, 6);
            buffer[6] = 0;
            u8g2.drawStr(38,27, buffer);
        #endif
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::idleScreen()
{
    displayMainScreen();

    current_screen_status = SCREEN_STATUS_IDLE;
}

void OLEDScreen::updateMainMenuPage()
{
    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,15, &(main_menu_line_1[main_menu_page_index - 1])[0]);
        u8g2.drawStr(0,32, &(main_menu_line_2[main_menu_page_index - 1])[0]);
        helperDrawImage32(main_menu_page_index - 1 );
    #else
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,20, &(main_menu_line_1[main_menu_page_index - 1])[0]);
        u8g2.drawStr(0,50, &(main_menu_line_2[main_menu_page_index - 1])[0]);
        helperDrawImage64(main_menu_page_index - 1);
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::updateSubFunctionPage()
{
    doValueSelection(USER_ACTION_NONE);
}

void OLEDScreen::updateSubWIFIModePage()
{
    u8g2.clearBuffer();

// TODO: Add a fancy wifi symbol like the cool TFT peeps

#if defined(HOME_WIFI_SSID) && defined(HOME_WIFI_PASSWORD)
#ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,15, "open http://");
        u8g2.drawStr(70,15, (String(wifi_hostname)+".local").c_str());
        u8g2.drawStr(0,32, "by browser");
#else
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,13, "open http://");
        u8g2.drawStr(0,33, (String(wifi_hostname)+".local").c_str());
        u8g2.drawStr(0,63, "by browser");
#endif
#else
#ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,15, wifi_ap_ssid);
        u8g2.drawStr(70,15, wifi_ap_password);
        u8g2.drawStr(0,32, wifi_ap_address);
#else
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,13, wifi_ap_ssid);
        u8g2.drawStr(0,33, wifi_ap_password);
        u8g2.drawStr(0,63, wifi_ap_address);
#endif
#endif
    u8g2.sendBuffer();
    updatecallback(USER_UPDATE_TYPE_WIFI);
}

void OLEDScreen::updateSubBindConfirmPage()
{
    // TODO: Put bind image?
    u8g2.clearBuffer();

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,15, "PRESS TO");
        u8g2.drawStr(70,15 , "SEND BIND");
        u8g2.drawStr(0,32, "REQUEST");
    #else
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,29, "PRESS TO SEND");
        u8g2.drawStr(0,59, "BIND REQUEST");
    #endif
    u8g2.sendBuffer();
}


void OLEDScreen::updateSubBindingPage()
{
    // TODO: Put bind image?
    u8g2.clearBuffer();

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,15, "BINDING");
    #else
        u8g2.setFont(u8g2_font_t0_17_mr);
        u8g2.drawStr(0,29, "BINDING");
    #endif
    u8g2.sendBuffer();


    updatecallback(USER_UPDATE_TYPE_BINDING);

    current_screen_status = SCREEN_STATUS_BINDING;
}

void OLEDScreen::doRateValueSelect(int action)
{
    nextIndex(current_rate_index, action, RATE_MAX_NUMBER);

    // TODO: Put bind image?
    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,15, &(rate_string[current_index])[0]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
        helperDrawImage32(IMAGE_RATE);
    #else
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,20, &(rate_string[current_index])[0]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,44, "PRESS TO");
        u8g2.drawStr(0,56, "CONFIRM");
        helperDrawImage64(IMAGE_RATE);
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::doPowerValueSelect(int action)
{
    nextIndex(current_power_index, action, MinPower, MaxPower+1);

    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,15, &(power_string[current_index])[0]);
        u8g2.setFont(u8g2_font_courR08_tr);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
        helperDrawImage32(IMAGE_POWER);
    #else
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,20, &(power_string[current_index])[0]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,44, "PRESS TO");
        u8g2.drawStr(0,56, "CONFIRM");
        helperDrawImage64(IMAGE_POWER);
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::doRatioValueSelect(int action)
{
    nextIndex(current_ratio_index, action, RATIO_MAX_NUMBER);

    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,15, &(ratio_string[current_index])[0]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
        helperDrawImage32(IMAGE_RATIO);
    #else
        u8g2.setFont(u8g2_font_t0_16_mr);
        u8g2.drawStr(0,20, &(ratio_string[current_index])[0]);
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(0,44, "PRESS TO");
        u8g2.drawStr(0,56, "CONFIRM");
        helperDrawImage64(IMAGE_RATIO);
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::doPowerSavingValueSelect(int action)
{
    nextIndex(current_smartfan_index, action, SMARTFAN_MAX_NUMBER);
    // TODO display the value
}

void OLEDScreen::doSmartFanValueSelect(int action)
{
    nextIndex(current_powersaving_index, action, POWERSAVING_MAX_NUMBER);
    // TODO display the value
}

void OLEDScreen::doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index)
{

    current_power_index = power_index;
    current_powersaving_index = motion_index;
    current_smartfan_index = fan_index;
    if(current_screen_status == SCREEN_STATUS_IDLE)
    {
        if(rate_index != current_rate_index)
        {
            current_rate_index = rate_index;
            displayMainScreen();
        }

        if(last_power_index != running_power_index || current_dynamic != dynamic)
        {
            current_dynamic = dynamic;
            last_power_index = running_power_index;
            displayMainScreen();
        }

        if(ratio_index != current_ratio_index)
        {
            current_ratio_index = ratio_index;
            displayMainScreen();
        }
    }
    else
    {
        last_power_index = running_power_index;
        current_dynamic = dynamic;
        current_rate_index = rate_index;
        current_ratio_index = ratio_index;
    }
}

void OLEDScreen::doTemperatureUpdate(uint8_t temperature)
{
    system_temperature = temperature;
    if(current_screen_status == SCREEN_STATUS_IDLE)
    {
        char buffer[20];
        strncpy(buffer, version, 6);
        sprintf(buffer+6, " %02d", system_temperature);
        // TODO
        // displayFontCenterWithCelsius(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2,
        //                     SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT,
        //                     String(buffer), TFT_WHITE,  COLOR_ELRS_BANNER_BACKGROUND);
    }
}

void OLEDScreen::doScreenBackLight(int state)
{
    #ifdef GPIO_PIN_OLED_BL
    digitalWrite(GPIO_PIN_OLED_BL, state);
    #endif
}

#endif
