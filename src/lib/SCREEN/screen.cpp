#include "screen.h"

#if defined(USE_OLED_SPI) || defined(USE_OLED_I2C) || defined(USE_OLED_SPI_SMALL) || defined(HAS_TFT_SCREEN)

void Screen::nullCallback(int updateType) {}
void (*Screen::updatecallback)(int updateType) = &nullCallback;


#if defined(RADIO_SX128X)
const char *Screen::rate_string[RATE_MAX_NUMBER] = {
    "500Hz",
    "250Hz",
    "150Hz",
    "50Hz"
};
#else
const char *Screen::rate_string[RATE_MAX_NUMBER] = {
    "200Hz",
    "100Hz",
    "50Hz",
    "25Hz"
};
#endif

const char *Screen::power_string[POWER_MAX_NUMBER] = {
    "10mW",
    "25mW",
    "50mW",
    "100mW",
    "250mW",
    "500mW",
    "1000mW",
    "2000mW"
};

const char *Screen::ratio_string[RATIO_MAX_NUMBER] = {
    "Off",
    "1:128",
    "1:64",
    "1:32",
    "1:16",
    "1:8",
    "1:4",
    "1:2"
};

const char *Screen::powersaving_string[POWERSAVING_MAX_NUMBER] = {
    "OFF",
    "ON"
};

const char *Screen::smartfan_string[SMARTFAN_MAX_NUMBER] = {
    "AUTO",
    "ON",
    "OFF"
};

const char *Screen::main_menu_line_1[] = {
    "PACKET",
    "TX",
    "TELEM",
    "MOTION",
    "FAN",
    "BIND",
    "UPDATE"
};

const char *Screen::main_menu_line_2[] = {
    "RATE",
    "POWER",
    "RATIO",
    "DETECT",
    "CONTROL",
    "MODE",
    "FW"
};

void Screen::doMainMenuPage(int action)
{
    int index = main_menu_page_index;
    if(action == USER_ACTION_UP)
    {
        index--;
        #ifndef HAS_THERMAL
        if (index == MAIN_MENU_SMARTFAN_INDEX) index--;
        #endif
        #ifndef HAS_GSENSOR
        if (index == MAIN_MENU_POWERSAVING_INDEX) index--;
        #endif
    }
    if(action == USER_ACTION_DOWN)
    {
        index++;
        #ifndef HAS_GSENSOR
        if (index == MAIN_MENU_POWERSAVING_INDEX) index++;
        #endif
        #ifndef HAS_THERMAL
        if (index == MAIN_MENU_SMARTFAN_INDEX) index++;
        #endif
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

    updateMainMenuPage();
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
        doMainMenuPage(USER_ACTION_NONE);
    }
    else if(current_page_index == PAGE_SUB_BIND_INDEX)
    {
        current_page_index = PAGE_MAIN_MENU_INDEX;
        doMainMenuPage(USER_ACTION_NONE);
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
        doMainMenuPage(USER_ACTION_NONE);
        updatecallback(USER_UPDATE_TYPE_EXIT_WIFI);
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
            updateSubFunctionPage();
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
        doMainMenuPage(USER_ACTION_NONE);
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
        current_rate_index = current_index;
        updatecallback(USER_UPDATE_TYPE_RATE);
    }
    else if(current_page_index == PAGE_SUB_POWER_INDEX)
    {
        current_power_index = current_index;
        updatecallback(USER_UPDATE_TYPE_POWER);
    }
    else if(current_page_index == PAGE_SUB_RATIO_INDEX)
    {
        current_ratio_index = current_index;
        updatecallback(USER_UPDATE_TYPE_RATIO);
    }
    else if(current_page_index == PAGE_SUB_SMARTFAN_INDEX)
    {
        current_smartfan_index = current_index;
        updatecallback(USER_UPDATE_TYPE_SMARTFAN);
    }
    else if(current_page_index == PAGE_SUB_POWERSAVING_INDEX)
    {
        current_powersaving_index = current_index;
        updatecallback(USER_UPDATE_TYPE_POWERSAVING);
    }

    doPageForward();
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

void Screen::activeScreen()
{
    doMainMenuPage(USER_ACTION_NONE);

    current_screen_status = SCREEN_STATUS_WORK;
    current_page_index = PAGE_MAIN_MENU_INDEX;
}

void Screen::doValueSelection(int action)
{
    if(current_page_index == PAGE_MAIN_MENU_INDEX)
    {
        doMainMenuPage(action);
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

void Screen::nextIndex(int &index, int action, int min, int max)
{
    if (action == USER_ACTION_NONE) // set current index to the value being processed
    {
        current_index = index;
    }
    else if (action == USER_ACTION_UP)
    {
        current_index--;
    }
    else if (action == USER_ACTION_DOWN)
    {
        current_index++;
    }

    if (current_index < min)
    {
        current_index = max - 1;
    }
    else if (current_index > max - 1)
    {
        current_index = min;
    }
}

void Screen::setInWifiMode()
{
    current_screen_status = SCREEN_STATUS_WORK;
    main_menu_page_index = MAIN_MENU_UPDATEFW_INDEX;
    current_page_index = PAGE_SUB_UPDATEFW_INDEX;
    updateSubWIFIModePage();
}

#endif
