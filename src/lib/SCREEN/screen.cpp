#include "screen.h"

void Screen::nullCallback(int updateType) {}
void (*Screen::updatecallback)(int updateType) = &nullCallback;

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
    updateMainMenuPage(USER_ACTION_NONE);

    current_screen_status = SCREEN_STATUS_WORK;
    current_page_index = PAGE_MAIN_MENU_INDEX;
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
