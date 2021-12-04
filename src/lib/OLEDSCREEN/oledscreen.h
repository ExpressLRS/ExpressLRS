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

#pragma once

#include "screen.h"

typedef enum
{
    MAIN_MENU_RATE_INDEX = 1,
    MAIN_MENU_POWER_INDEX = 2,
    MAIN_MENU_RATIO_INDEX = 3,
    MAIN_MENU_POWERSAVING_INDEX = 4,
    MAIN_MENU_SMARTFAN_INDEX =5,

    MAIN_MENU_BIND_INDEX = 6,
    MAIN_MENU_UPDATEFW_INDEX = 7
}Screen_MainMenuIndex_t;

typedef enum
{
    PAGE_MAIN_MENU_INDEX = 0,
    PAGE_SUB_RATE_INDEX = 1,
    PAGE_SUB_POWER_INDEX = 2,
    PAGE_SUB_RATIO_INDEX = 3,
    PAGE_SUB_POWERSAVING_INDEX = 4,
    PAGE_SUB_SMARTFAN_INDEX = 5,

    PAGE_SUB_BIND_INDEX = 6,
    PAGE_SUB_UPDATEFW_INDEX = 7,
   
    PAGE_SUB_BINDING_INDEX = 16
} Screen_PageIndex_t;

typedef enum
{
    USER_ACTION_NONE = 0,
    USER_ACTION_UP = 1,
    USER_ACTION_DOWN = 2,
    USER_ACTION_LEFT = 3,
    USER_ACTION_RIGHT = 4,
    USER_ACTION_CONFIRM = 5
} Screen_Action_t;

typedef enum
{
    SCREEN_STATUS_INIT = 0,
    SCREEN_STATUS_IDLE = 1,
    SCREEN_STATUS_WORK = 2,
    SCREEN_STATUS_BINDING = 3
} Screen_Status_t;


typedef enum
{
    USER_POWERSAVING_OFF = 0,
    USER_POWERSAVING_ON = 1
} Screen_User_PowerSaving_t;

typedef enum
{
    USER_SMARTFAN_AUTO = 0,
    USER_SMARTFAN_ALAYWS_ON = 1,
    USER_SMARTFAN_OFF = 2
} Screen_User_SMARTFAN_t;

typedef enum
{
    
    USER_UPDATE_TYPE_RATE = 0,
    USER_UPDATE_TYPE_POWER = 1,
    USER_UPDATE_TYPE_RATIO = 2,
    USER_UPDATE_TYPE_POWERSAVING = 3,
    USER_UPDATE_TYPE_SMARTFAN = 4,
    USER_UPDATE_TYPE_BINDING = 5,
    USER_UPDATE_TYPE_EXIT_BINDING = 6,
    USER_UPDATE_TYPE_WIFI = 7,
    USER_UPDATE_TYPE_EXIT_WIFI = 8
} Screen_Update_Type_t;

typedef enum
{
    SCREEN_BACKLIGHT_ON = 0,
    SCREEN_BACKLIGHT_OFF = 1
} Screen_BackLight_t;

class OLEDScreen: public Screen
{
private:
    void doValueSelection(int action);
    void doRateValueSelect(int action);
    void doPowerValueSelect(int action); 
    void doRatioValueSelect(int action);

    void updateMainMenuPage(int action);
    void updateSubFunctionPage(int action);
    void updateSubWIFIModePage();
    void updateSubBindConfirmPage();
    void updateSubBindingPage();

    void doPageBack();
    void doPageForward();
    void doValueConfirm();        


public:

    void init();
    void doUserAction(int action);
    void idleScreen();
    void activeScreen();
    static void nullCallback(int updateType);
    static void (*updatecallback)(int updateType);
    void doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index);
};