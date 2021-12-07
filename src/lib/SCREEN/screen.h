
#pragma once

#include "targets.h"

#define RATE_MAX_NUMBER 4
#define POWER_MAX_NUMBER 8
#define RATIO_MAX_NUMBER 8
#define POWERSAVING_MAX_NUMBER 2
#define SMARTFAN_MAX_NUMBER 3

#define VERSION_MAX_LENGTH  6

#define IMAGE_RATE 0
#define IMAGE_POWER 1
#define IMAGE_RATIO 2
#define IMAGE_MOTION 3
#define IMAGE_FAN 4
#define IMAGE_BIND 5
#define IMAGE_WIFI 6


typedef enum
{
    MAIN_MENU_RATE_INDEX = 1,
    MAIN_MENU_POWER_INDEX,
    MAIN_MENU_RATIO_INDEX,
    MAIN_MENU_POWERSAVING_INDEX,
    MAIN_MENU_SMARTFAN_INDEX,
    MAIN_MENU_BIND_INDEX,
    MAIN_MENU_UPDATEFW_INDEX
} Screen_MainMenuIndex_t;

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

class Screen
{
private:

    void doMainMenuPage(int action);
    void doPageBack();
    void doPageForward();
    void doValueConfirm();

protected:
    // Common Variables
    int current_page_index;
    int main_menu_page_index;
    int current_rate_index;
    int current_power_index;
    int current_ratio_index;
    int current_powersaving_index;
    int current_smartfan_index;

    int current_screen_status;

    uint8_t system_temperature;

    void nextIndex(int &index, int action, int min, int max);
    void nextIndex(int &index, int action, int max) { nextIndex(index, action, 0, max); }
    void doValueSelection(int action);

    virtual void doRateValueSelect(int action) = 0;
    virtual void doPowerValueSelect(int action) = 0;
    virtual void doRatioValueSelect(int action) = 0;
    virtual void doPowerSavingValueSelect(int action) = 0;
    virtual void doSmartFanValueSelect(int action) = 0;

    virtual void updateMainMenuPage() = 0;
    virtual void updateSubFunctionPage() = 0;
    virtual void updateSubWIFIModePage() = 0;
    virtual void updateSubBindConfirmPage() = 0;
    virtual void updateSubBindingPage() = 0;


    static const char *rate_string[RATE_MAX_NUMBER];
    static const char *power_string[POWER_MAX_NUMBER];
    static const char *ratio_string[RATIO_MAX_NUMBER];
    static const char *powersaving_string[POWERSAVING_MAX_NUMBER];
    static const char *smartfan_string[SMARTFAN_MAX_NUMBER];
    static const char *main_menu_line_1[];
    static const char *main_menu_line_2[];
    static const char thisVersion[];

public:
    static void nullCallback(int updateType);
    static void (*updatecallback)(int updateType);

    virtual void init() = 0;
    virtual void idleScreen() = 0;
    virtual void doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index) = 0;
    virtual void doTemperatureUpdate(uint8_t temperature) = 0;
    virtual void doScreenBackLight(int state) = 0;

    void activeScreen();
    void doUserAction(int action);

    int getUserRateIndex() { return current_rate_index; }
    int getUserPowerIndex() { return current_power_index; }
    int getUserRatioIndex() { return current_ratio_index; }
    int getScreenStatus() { return current_screen_status; }
    int getUserPowerSavingIndex() { return current_powersaving_index; }
    int getUserSmartFanIndex() { return current_smartfan_index; }
};