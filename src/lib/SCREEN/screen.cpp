#ifdef TARGET_AXIS_THOR_2400_TX

#include "screen.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define RATE_MAX_NUMBER 4
#define POWER_MAX_NUMBER 7
#define RATIO_MAX_NUMBER 8
#define POWERSAVING_MAX_NUMBER 2
#define SMARTFAN_MAX_NUMBER 3

#define VERSION_MAX_LENGTH  6

boolean is_confirmed = false;

void Screen::nullCallback(int updateType) {}
void (*Screen::updatecallback)(int updateType) = &nullCallback;

String rate_string[RATE_MAX_NUMBER] = {
    "500HZ",
    "250HZ",
    "150HZ",
    "50HZ"
};

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

String powersaving_string[POWERSAVING_MAX_NUMBER] = {
    "OFF",
    "ON"
};

String smartfan_string[SMARTFAN_MAX_NUMBER] = {
    "AUTO",
    "ON",
    "OFF"
};

const uint16_t *main_menu_icons[] = {
    elrs_rate,
    elrs_power,
    elrs_ratio,
    elrs_motion,
    elrs_fan,
    elrs_bind,
    elrs_updatefw
};

String main_menu_line_1[] = {
    "PACKET",
    "TX",
    "TELEM",
    "MOTION",
    "FAN",
    "BIND",
    "UPDATE"
};

String main_menu_line_2[] = {
    "RATE",
    "POWER",
    "RATIO",
    "DETECT",
    "CONTROL",
    "MODE",
    "FW"
};

static char thisVersion[] = {LATEST_VERSION, 0};

void Screen::init()
{ 
    tft.init();
    doScreenBackLight(HIGH);

    tft.fillScreen(TFT_WHITE);

    tft.setRotation(1);

    tft.setSwapBytes(true);

    tft.pushImage(0, 0, INIT_PAGE_LOGO_X, INIT_PAGE_LOGO_Y, axis_logo);

    tft.fillRect(SCREEN_FONT_GAP, INIT_PAGE_FONT_START_Y - INIT_PAGE_FONT_PADDING, 
                    SCREEN_X - SCREEN_FONT_GAP*2, SCREEN_NORMAL_FONT_SIZE + INIT_PAGE_FONT_PADDING*2, TFT_BLACK);

    if(strlen(thisVersion) > VERSION_MAX_LENGTH)
    {
        thisVersion[VERSION_MAX_LENGTH] = 0x00;
    }
    char buffer[50];
    sprintf(buffer, "THOR-%s  ELRS-%s", STRING_THOR_VERSION, thisVersion);
    displayFontCenter(INIT_PAGE_FONT_START_X, SCREEN_X - INIT_PAGE_FONT_START_X, INIT_PAGE_FONT_START_Y, 
                        SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        String(buffer), TFT_WHITE, TFT_BLACK);

    doScreenBackLight(LOW);

    current_screen_status = SCREEN_STATUS_INIT;

    current_rate_index = 0;
    current_power_index = 0;
    current_ratio_index = 0;
    current_powersaving_index = 0;
    current_smartfan_index = 0;

    current_page_index = PAGE_MAIN_MENU_INDEX;

    main_menu_page_index = MAIN_MENU_RATE_INDEX;

    system_temperature = 25;
    
}

void Screen::idleScreen()
{
    tft.fillRect(0, 0, SCREEN_X/2, SCREEN_Y, COLOR_ELRS_BANNER_BACKGROUND);
    tft.fillRect(SCREEN_X/2, 0, SCREEN_X/2, SCREEN_Y, TFT_WHITE);

    tft.pushImage(IDLE_PAGE_START_X, IDLE_PAGE_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_banner);

    char buffer[20];
    sprintf(buffer, "%s %02d", thisVersion, system_temperature);
    displayFontCenterWithCelsius(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2, 
                                SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT, 
                                String(buffer), TFT_WHITE,  COLOR_ELRS_BANNER_BACKGROUND);
 
    displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATE_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        rate_string[current_rate_index], TFT_BLACK, TFT_WHITE);

    displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_POWER_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        power_string[current_power_index], TFT_BLACK, TFT_WHITE);

    displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATIO_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);

    current_screen_status = SCREEN_STATUS_IDLE;
}

void Screen::activeScreen()
{
    updateMainMenuPage(USER_ACTION_NONE);

    current_screen_status = SCREEN_STATUS_WORK;
    current_page_index = PAGE_MAIN_MENU_INDEX;

}

void Screen::doUserAction(int action)
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

void Screen::doPageBack()
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        idleScreen();
    }
    else if((current_page_index == PAGE_SUB_RATE_INDEX) ||
            (current_page_index == PAGE_SUB_POWER_INDEX) ||
            (current_page_index == PAGE_SUB_RATIO_INDEX) ||
            (current_page_index == PAGE_SUB_POWERSAVING_INDEX) ||
            (current_page_index == PAGE_SUB_SMARTFAN_INDEX))
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
        current_page_index = PAGE_MAIN_MENU_INDEX;
        updateMainMenuPage(USER_ACTION_NONE);
        updatecallback(USER_UPDATE_TYPE_EXIT_BINDING);
    }          
}


void Screen::doPageForward()
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        if((main_menu_page_index == MAIN_MENU_RATE_INDEX) ||
            (main_menu_page_index == MAIN_MENU_POWER_INDEX) ||
            (main_menu_page_index == MAIN_MENU_RATIO_INDEX) ||
            (main_menu_page_index == MAIN_MENU_POWERSAVING_INDEX) ||
            (main_menu_page_index == MAIN_MENU_SMARTFAN_INDEX))
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
            (current_page_index == PAGE_SUB_RATIO_INDEX) ||
            (current_page_index == PAGE_SUB_POWERSAVING_INDEX) ||
            (current_page_index == PAGE_SUB_SMARTFAN_INDEX))
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

void Screen::doValueConfirm()
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
    else if(current_page_index == PAGE_SUB_SMARTFAN_INDEX)
    {
        updatecallback(USER_UPDATE_TYPE_SMARTFAN);
    }
    else if(current_page_index == PAGE_SUB_POWERSAVING_INDEX)
    {
        updatecallback(USER_UPDATE_TYPE_POWERSAVING);
    }

    doPageForward();
}

void Screen::updateMainMenuPage(int action)
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

    tft.fillScreen(TFT_WHITE);

    tft.pushImage(MAIN_PAGE_ICON_START_X, MAIN_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, main_menu_icons[main_menu_page_index-1]);


    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        main_menu_line_1[main_menu_page_index - 1], TFT_BLACK, TFT_WHITE);

    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        main_menu_line_2[main_menu_page_index - 1], TFT_BLACK, TFT_WHITE);
}

void Screen::updateSubFunctionPage(int action)
{
    tft.fillScreen(TFT_WHITE);

    doValueSelection(action);

    displayFontCenter(SUB_PAGE_TIPS_START_X, SCREEN_X, SUB_PAGE_TIPS_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        "PRESS TO CONFIRM", TFT_BLACK, TFT_WHITE);
}

void Screen::updateSubWIFIModePage()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_wifimode);
#if defined(HOME_WIFI_SSID) && defined(HOME_WIFI_PASSWORD)
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "open http://", TFT_BLACK, TFT_WHITE);

    String host_msg = String(STRING_WEB_UPDATE_TX_HOST) + ".local";
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        host_msg, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "by browser", TFT_BLACK, TFT_WHITE);
#else
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        STRING_WEB_UPDATE_TX_SSID, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        STRING_WEB_UPDATE_PWD, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                        STRING_WEB_UPDATE_IP, TFT_BLACK, TFT_WHITE);
#endif
    updatecallback(USER_UPDATE_TYPE_WIFI);                    


}

void Screen::updateSubBindConfirmPage()
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

void Screen::updateSubBindingPage()
{
    tft.fillScreen(TFT_WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        "BINDING", TFT_BLACK, TFT_WHITE);

    updatecallback(USER_UPDATE_TYPE_BINDING);                        
}

void Screen::doValueSelection(int action)
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
    else if(current_page_index == PAGE_SUB_POWERSAVING_INDEX)
    {
        doPowerSavingValueSelect(action);
    }
    else if(current_page_index == PAGE_SUB_SMARTFAN_INDEX)
    {
        doSmartFanValueSelect(action);
    }
}

void Screen::doRateValueSelect(int action)
{
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
    
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        rate_string[current_rate_index], TFT_BLACK, TFT_WHITE);

}

void Screen::doPowerValueSelect(int action)
{
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

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        power_string[current_power_index], TFT_BLACK, TFT_WHITE);
}

void Screen::doRatioValueSelect(int action)
{
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

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);

}

void Screen::doPowerSavingValueSelect(int action)
{
    int index = current_powersaving_index;

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
        index = POWERSAVING_MAX_NUMBER -1;
    }
    if(index > POWERSAVING_MAX_NUMBER -1)
    {
        index = 0;
    }

    current_powersaving_index = index;

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        powersaving_string[current_powersaving_index], TFT_BLACK, TFT_WHITE);    
}

void Screen::doSmartFanValueSelect(int action)
{
    int index = current_smartfan_index;

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
        index = SMARTFAN_MAX_NUMBER -1;
    }
    if(index > SMARTFAN_MAX_NUMBER -1)
    {
        index = 0;
    }

    current_smartfan_index = index;

    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT, 
                        smartfan_string[current_smartfan_index], TFT_BLACK, TFT_WHITE);    
}

void Screen::doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index)
{
    if(current_screen_status == SCREEN_STATUS_IDLE)
    {
        if(rate_index != current_rate_index)
        {
            current_rate_index = rate_index;
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATE_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                                rate_string[current_rate_index], TFT_BLACK, TFT_WHITE);
        }
         
        if(power_index != current_power_index)
        {
            current_power_index = power_index;
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_POWER_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                                power_string[current_power_index], TFT_BLACK, TFT_WHITE);
        }

        if(ratio_index != current_ratio_index)
        {
            current_ratio_index = ratio_index;
            displayFontCenter(IDLE_PAGE_STAT_START_X, SCREEN_X, IDLE_PAGE_RATIO_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT, 
                                ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);
        }
    }
    else
    {
        current_rate_index = rate_index;
        current_power_index = power_index;
        current_ratio_index = ratio_index;
    }

    current_powersaving_index = motion_index;
    current_smartfan_index = fan_index;
}

void Screen::doTemperatureUpdate(uint8_t temperature)
{
    system_temperature = temperature;
    if(current_screen_status == SCREEN_STATUS_IDLE)
    {
        char buffer[20];
        sprintf(buffer, "%s %02d", thisVersion, system_temperature);
        displayFontCenterWithCelsius(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2, 
                            SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT, 
                            String(buffer), TFT_WHITE,  COLOR_ELRS_BANNER_BACKGROUND);
    }

}

void Screen::displayFontCenterWithCelsius(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y, 
                                            int font_size, int font_type, String font_string, 
                                            uint16_t fgColor, uint16_t bgColor)
{
    tft.fillRect(font_start_x, font_start_y, font_end_x - font_start_x, font_size, bgColor);

    int start_pos = font_start_x + (font_end_x - font_start_x -  tft.textWidth(font_string, font_type) - font_size)/2;

    tft.setCursor(start_pos, font_start_y, font_type);
    tft.setTextColor(fgColor, bgColor);
    tft.print(font_string);

    int celsius_start_pos = start_pos + tft.textWidth(font_string, font_type);
    if(font_size == SCREEN_SMALL_FONT_SIZE)
    {
        tft.pushImage(celsius_start_pos, font_start_y, font_size/2, font_size, Celsius4x8);
    }

    int char_c_pos = celsius_start_pos + font_size/2;
    tft.setCursor(char_c_pos, font_start_y, font_type);
    tft.print("C");
}

void Screen::displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y, 
                                            int font_size, int font_type, String font_string, 
                                            uint16_t fgColor, uint16_t bgColor)
{
    tft.fillRect(font_start_x, font_start_y, font_end_x - font_start_x, font_size, bgColor);

    int start_pos = font_start_x + (font_end_x - font_start_x -  tft.textWidth(font_string, font_type))/2;
    tft.setCursor(start_pos, font_start_y, font_type);

    tft.setTextColor(fgColor, bgColor);
    tft.print(font_string);
}

int Screen::getUserRateIndex()
{
    return current_rate_index;
}

int Screen::getUserPowerIndex()
{
    return current_power_index;
}

int Screen::getUserRatioIndex()
{
    return current_ratio_index;
}

int Screen::getUserPowerSavingIndex()
{
    return current_powersaving_index;
}

int Screen::getUserSmartFanIndex()
{
    return current_smartfan_index;
}

int Screen::getScreenStatus()
{
    return current_screen_status;
}

void Screen::doScreenBackLight(int state)
{
     digitalWrite(TFT_BL, state);
}
#endif
