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
// OLED specific header files.
#include "oledscreen.h"
#include <U8g2lib.h>    // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO
#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI

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

#define RATE_MAX_NUMBER 4
#define POWER_MAX_NUMBER 7
#define RATIO_MAX_NUMBER 8

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
void displayLogo()
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

void displayFontCenter(const char * info)
{ 

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_courR08_tr);
#ifdef USE_OLED_SPI_SMALL
    u8g2.drawStr(32, 32, info);
#else
    u8g2.drawStr(64, 64, info);
#endif
    u8g2.sendBuffer();

}


#ifdef Regulatory_Domain_ISM_2400
String rate_string[RATE_MAX_NUMBER] = {
    "500HZ",
    "250HZ",
    "150HZ",
    "50HZ"
};
#else 
String rate_string[RATE_MAX_NUMBER] = {
    "200HZ",
    "100HZ",
    "50HZ",
    "25HZ"
};
#endif

String power_string[POWER_MAX_NUMBER] = {
    "10mW",
    "25mW",
    "50mW",
    "100mW",
    "250mW",
    "500mW",
    "1000mW"
};

String ratio_string[RATIO_MAX_NUMBER] = {
    "Off",
    "1:128",
    "1:64",
    "1:32",
    "1:16",
    "1:8",
    "1:4",
    "1:2"
};



String main_menu_line_1[] = {
    "PACKET",
    "TX",
    "TELEM",
    "BIND",
    "UPDATE"
};

String main_menu_line_2[] = {
    "RATE",
    "POWER",
    "RATIO",
    "MODE",
    "FW"
};




// static char thisVersion[] = {LATEST_VERSION, 0};

void OLEDScreen::nullCallback(int updateType) {}
void (*OLEDScreen::updatecallback)(int updateType) = &nullCallback;

void OLEDScreen::init()
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

void OLEDScreen::idleScreen()
{

    u8g2.clearBuffer();

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(70,15 ,&(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,32, &(power_string[current_power_index])[0]);
        u8g2.drawStr(70,32, "Test");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, "ExpressLRS");
        u8g2.drawStr(0,42, &(rate_string[current_rate_index])[0]); 
        u8g2.drawStr(70,42 , &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,57, &(power_string[current_power_index])[0]);
        u8g2.setFont(u8g2_font_courR08_tr);
        u8g2.drawStr(70,53, "TLM");
        u8g2.drawStr(0,24, "Ver: ");
        u8g2.drawStr(38,24, "TEST");
    #endif
    u8g2.sendBuffer();
    
    current_screen_status = SCREEN_STATUS_IDLE;
}

void OLEDScreen::activeScreen()
{
    updateMainMenuPage(USER_ACTION_NONE);

    current_screen_status = SCREEN_STATUS_WORK;
    current_page_index = PAGE_MAIN_MENU_INDEX;

}

void OLEDScreen::doUserAction(int action)
{
    if(action == USER_ACTION_LEFT)
    {
        doPageBack();
    }
    else if(action == USER_ACTION_RIGHT)
    {
        doPageForward();
    }
    else if(action == USER_ACTION_CONFIRM)
    {
        doValueConfirm();
    }
    else
    {
        doValueSelection(action);
    }
}

void OLEDScreen::doPageBack()
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        idleScreen();
    }
    else if((current_page_index == PAGE_SUB_RATE_INDEX) ||
            (current_page_index == PAGE_SUB_POWER_INDEX) ||
            (current_page_index == PAGE_SUB_RATIO_INDEX)) 
    {
        current_page_index = PAGE_MAIN_MENU_INDEX;
        updateMainMenuPage(USER_ACTION_NONE);
    }
    else if(current_page_index == PAGE_SUB_BIND_INDEX)
    {
        current_page_index = PAGE_MAIN_MENU_INDEX;
        updateMainMenuPage(USER_ACTION_NONE);
    }
    else if(current_page_index == PAGE_SUB_BINDING_INDEX)
    {
        current_page_index = PAGE_SUB_BIND_INDEX;
        updateSubBindConfirmPage();
        updatecallback(USER_UPDATE_TYPE_EXIT_BINDING);

        current_screen_status = SCREEN_STATUS_WORK;
    }
    else if(current_page_index == PAGE_SUB_UPDATEFW_INDEX)
    {
        current_page_index = PAGE_MAIN_MENU_INDEX;
        updateMainMenuPage(USER_ACTION_NONE);
        updatecallback(USER_UPDATE_TYPE_EXIT_WIFI);
    }
}


void OLEDScreen::doPageForward()
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        if((main_menu_page_index == MAIN_MENU_RATE_INDEX) ||
            (main_menu_page_index == MAIN_MENU_POWER_INDEX) ||
            (main_menu_page_index == MAIN_MENU_RATIO_INDEX))
        {
            current_page_index = main_menu_page_index;
            updateSubFunctionPage(USER_ACTION_NONE);
        }
        else if(main_menu_page_index == MAIN_MENU_UPDATEFW_INDEX)
        {
            current_page_index = PAGE_SUB_UPDATEFW_INDEX;
            updateSubWIFIModePage();
        }
        else if(main_menu_page_index == MAIN_MENU_BIND_INDEX)
        {
            current_page_index = PAGE_SUB_BIND_INDEX;
            updateSubBindConfirmPage();
        }
    }
    else if((current_page_index == PAGE_SUB_RATE_INDEX) ||
            (current_page_index == PAGE_SUB_POWER_INDEX) ||
            (current_page_index == PAGE_SUB_RATIO_INDEX))
    {
        current_page_index = PAGE_MAIN_MENU_INDEX;
        updateMainMenuPage(USER_ACTION_NONE);
    }
    else if(current_page_index == PAGE_SUB_BIND_INDEX)
    {
        current_page_index = PAGE_SUB_BINDING_INDEX;
        updateSubBindingPage();
    }  
}

void OLEDScreen::doValueConfirm()
{
    if(current_page_index == PAGE_SUB_RATE_INDEX)
    {
        updatecallback(USER_UPDATE_TYPE_RATE);
    }
    else if(current_page_index == PAGE_SUB_POWER_INDEX)
    {
        updatecallback(USER_UPDATE_TYPE_POWER);
    }
    else if(current_page_index == PAGE_SUB_RATIO_INDEX)
    {
        updatecallback(USER_UPDATE_TYPE_RATIO);
    }

    doPageForward();
}

void OLEDScreen::updateMainMenuPage(int action)
{
    int index = main_menu_page_index;
    if(action == USER_ACTION_UP)
    {
        index--;
    }
    if(action == USER_ACTION_DOWN)
    {
        index++;
    }
    if(index < MAIN_MENU_RATE_INDEX)
    {
        index = MAIN_MENU_UPDATEFW_INDEX;
    }
    if(index > MAIN_MENU_UPDATEFW_INDEX)
    {
        index = MAIN_MENU_RATE_INDEX;
    }
    main_menu_page_index = index;
    
    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(main_menu_line_1[main_menu_page_index - 1])[0]);
        u8g2.drawStr(0,32, &(main_menu_line_2[main_menu_page_index - 1])[0]);
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(main_menu_line_1[main_menu_page_index - 1])[0]);
        u8g2.drawStr(0,60, &(main_menu_line_2[main_menu_page_index - 1])[0]);
    #endif
    u8g2.sendBuffer();

}

void OLEDScreen::updateSubFunctionPage(int action)
{

    doValueSelection(action);

}

void OLEDScreen::updateSubWIFIModePage()
{

    u8g2.clearBuffer();

// TODO: Add a fancy wifi symbol like the cool TFT peeps

#if defined(HOME_WIFI_SSID) && defined(HOME_WIFI_PASSWORD)
#ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, "open http://");
        u8g2.drawStr(70,15 , host_msg);
        u8g2.drawStr(0,32, "by browser");
#else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, "open http://");
        u8g2.drawStr(0,30, host_msg);
        u8g2.drawStr(0,60, "by browser");
#endif
#else
#ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, "ExpressLRS TX");
        u8g2.drawStr(70,15 , "expresslrs");
        u8g2.drawStr(0,32, "10.0.0.1");
#else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, "ExpressLRS TX");
        u8g2.drawStr(0,30, "expresslrs");
        u8g2.drawStr(0,60, "10.0.0.1");
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
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, "PRESS TO");
        u8g2.drawStr(70,15 , "SEND BIND");
        u8g2.drawStr(0,32, "REQUEST");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, "PRESS TO SEND");
        u8g2.drawStr(32, 10, "BIND REQUEST");
    #endif
    u8g2.sendBuffer();
}


void OLEDScreen::updateSubBindingPage()
{
    // TODO: Put bind image? 
    u8g2.clearBuffer();

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, "BINDING");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, "BINDING");
    #endif
    u8g2.sendBuffer();


    updatecallback(USER_UPDATE_TYPE_BINDING);

    current_screen_status = SCREEN_STATUS_BINDING;
    
}

void OLEDScreen::doValueSelection(int action)
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        updateMainMenuPage(action);
    }
    else if(current_page_index == PAGE_SUB_RATE_INDEX)
    {
        doRateValueSelect(action);
    }
    else if(current_page_index == PAGE_SUB_POWER_INDEX)
    {
        doPowerValueSelect(action);
    }
    else if(current_page_index == PAGE_SUB_RATIO_INDEX)
    {
        doRatioValueSelect(action);
    }
}

void OLEDScreen::doRateValueSelect(int action)
{
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();

    int index = current_rate_index;

    if(action == USER_ACTION_UP)
    {
        index++;
    }
    if(action == USER_ACTION_DOWN)
    {
        index--;
    }

    if(index < 0)
    {
        index = RATE_MAX_NUMBER -1;
    }
    if(index > RATE_MAX_NUMBER -1)
    {
        index = 0;
    }

    current_rate_index = index;

    // TODO: Put bind image? 
    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(rate_string[current_rate_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();
    

}

void OLEDScreen::doPowerValueSelect(int action)
{

    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(power_string[current_power_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(power_string[current_power_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();

    int index = current_power_index;

    if(action == USER_ACTION_UP)
    {
        index--;
    }
    if(action == USER_ACTION_DOWN)
    {
        index++;
    }

    if(index < 0)
    {
        index = POWER_MAX_NUMBER -1;
    }
    if(index > POWER_MAX_NUMBER -1)
    {
        index = 0;
    }

    current_power_index = index;

    u8g2.clearBuffer();


    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(power_string[current_power_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(power_string[current_power_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();
}

void OLEDScreen::doRatioValueSelect(int action)
{

    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();

    int index = current_ratio_index;

    if(action == USER_ACTION_UP)
    {
        index++;
    }
    if(action == USER_ACTION_DOWN)
    {
        index--;
    }

    if(index < 0)
    {
        index = RATIO_MAX_NUMBER -1;
    }
    if(index > RATIO_MAX_NUMBER -1)
    {
        index = 0;
    }

    current_ratio_index = index;

    // displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
    //                     ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);

    u8g2.clearBuffer();
    #ifdef USE_OLED_SPI_SMALL
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #else
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,10, &(ratio_string[current_ratio_index])[0]);
        u8g2.drawStr(0,60, "PRESS TO CONFIRM");
    #endif
    u8g2.sendBuffer();

}

void OLEDScreen::doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index)
{
    if(current_screen_status == SCREEN_STATUS_IDLE)
    {
        if(rate_index != current_rate_index)
        {
            current_rate_index = rate_index;
            // displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATE_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
            //                     rate_string[current_rate_index], TFT_BLACK, TFT_WHITE);
            u8g2.clearBuffer();
            #ifdef USE_OLED_SPI_SMALL
                u8g2.setFont(u8g2_font_courR10_tr);
                u8g2.drawStr(0,15, &(rate_string[current_rate_index])[0]);
            #else
                u8g2.setFont(u8g2_font_courR10_tr);
                u8g2.drawStr(0,10, &(rate_string[current_rate_index])[0]);
            #endif
            u8g2.sendBuffer();
        }
         
        if(power_index != current_power_index)
        {
            current_power_index = power_index;
            // displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_POWER_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
            //                     power_string[current_power_index], TFT_BLACK, TFT_WHITE);
            u8g2.clearBuffer();
            #ifdef USE_OLED_SPI_SMALL
                u8g2.setFont(u8g2_font_courR10_tr);
                u8g2.drawStr(0,15, &(power_string[current_power_index])[0]);
            #else
                u8g2.setFont(u8g2_font_courR10_tr);
                u8g2.drawStr(0,10, &(power_string[current_power_index])[0]);
            #endif
            u8g2.sendBuffer();

        }

        if(ratio_index != current_ratio_index)
        {
            current_ratio_index = ratio_index;
            // displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATIO_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
            //                     ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);
            u8g2.clearBuffer();
            #ifdef USE_OLED_SPI_SMALL
            u8g2.setFont(u8g2_font_courR10_tr);
            u8g2.drawStr(0,15, &(ratio_string[current_ratio_index])[0]);
            #else
            u8g2.setFont(u8g2_font_courR10_tr);
            u8g2.drawStr(0,10, &(ratio_string[current_ratio_index])[0]);
            #endif
            u8g2.sendBuffer();

        }
    }
    else
    {
        current_rate_index = rate_index;
        current_power_index = power_index;
        current_ratio_index = ratio_index;
    }
}

#endif