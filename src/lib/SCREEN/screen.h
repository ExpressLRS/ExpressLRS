#pragma once

#include "targets.h"
#include "logos.h"
#include "TFT_eSPI_User_Setup.h"
#include <SPI.h>


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

class Screen
{
private:
    int current_page_index;
    int main_menu_page_index;
    int current_rate_index;
    int current_power_index;
    int current_ratio_index;
    int current_powersaving_index;
    int current_smartfan_index;

    int current_screen_status;

    uint8_t system_temperature;  

    void doValueSelection(int action);

    void doRateValueSelect(int action);
    void doPowerValueSelect(int action); 
    void doRatioValueSelect(int action);
    void doPowerSavingValueSelect(int action);
    void doSmartFanValueSelect(int action);

    void updateMainMenuPage(int action);
    void updateSubFunctionPage(int action);
    void updateSubWIFIModePage();
    void updateSubBindConfirmPage();
    void updateSubBindingPage();

    void doPageBack();
    void doPageForward();
    void doValueConfirm();

    void displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y, 
                                            int font_size, int font_type, String font_string, 
                                            uint16_t fgColor, uint16_t bgColor); 

    void displayFontCenterWithCelsius(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y, 
                                            int font_size, int font_type, String font_string, 
                                            uint16_t fgColor, uint16_t bgColor);                         

public:

    void init();
    void activeScreen();
    void idleScreen();
    void doUserAction(int action);
    void doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index);
    void doTemperatureUpdate(uint8_t temperature);
    void doScreenBackLight(int state);
    static void nullCallback(int updateType);
    static void (*updatecallback)(int updateType);
    int getUserRateIndex();
    int getUserPowerIndex();
    int getUserRatioIndex();
    int getUserPowerSavingIndex();
    int getUserSmartFanIndex();
    int getScreenStatus();

};

#define COLOR_ELRS_BANNER_BACKGROUND    0x9E2D

#define SCREEN_X    TFT_HEIGHT
#define SCREEN_Y    TFT_WIDTH

//see #define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define SCREEN_SMALL_FONT_SIZE      8
#define SCREEN_SMALL_FONT           1   

//see #define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define SCREEN_NORMAL_FONT_SIZE     16
#define SCREEN_NORMAL_FONT          2

//see #define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define SCREEN_LARGE_FONT_SIZE      26
#define SCREEN_LARGE_FONT           4

//ICON SIZE Definition
#define SCREEN_LARGE_ICON_SIZE      60
#define SCREEN_NORAML_ICON_SIZE     20
#define SCREEN_NARROW_ICON_WEIGHT   14

//GAP Definition
#define SCREEN_CONTENT_GAP          10
#define SCREEN_FONT_GAP             5

//INIT LOGO PAGE Definition
#define INIT_PAGE_LOGO_X            SCREEN_X
#define INIT_PAGE_LOGO_Y            53
#define INIT_PAGE_FONT_PADDING      3
#define INIT_PAGE_FONT_START_X      SCREEN_FONT_GAP
#define INIT_PAGE_FONT_START_Y      INIT_PAGE_LOGO_Y + (SCREEN_Y - INIT_PAGE_LOGO_Y - SCREEN_NORMAL_FONT_SIZE)/2

//IDLE PAGE Definition
#define IDLE_PAGE_START_X   SCREEN_CONTENT_GAP
#define IDLE_PAGE_START_Y   0

#define IDLE_PAGE_STAT_START_X  SCREEN_X/2 + SCREEN_CONTENT_GAP
#define IDLE_PAGE_STAT_Y_GAP    (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE * 3)/4

#define IDLE_PAGE_RATE_START_Y  IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_POWER_START_Y IDLE_PAGE_RATE_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP
#define IDLE_PAGE_RATIO_START_Y IDLE_PAGE_POWER_START_Y + SCREEN_NORMAL_FONT_SIZE + IDLE_PAGE_STAT_Y_GAP

//MAIN PAGE Definition
#define MAIN_PAGE_ICON_START_X  SCREEN_CONTENT_GAP
#define MAIN_PAGE_ICON_START_Y  (SCREEN_Y -  SCREEN_LARGE_ICON_SIZE)/2

#define MAIN_PAGE_WORD_START_X  MAIN_PAGE_ICON_START_X + SCREEN_LARGE_ICON_SIZE + SCREEN_CONTENT_GAP
#define MAIN_PAGE_WORD_START_Y  (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE)/2
#define MAIN_PAGE_WORD_START_Y1 (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE*2 - SCREEN_FONT_GAP)/2
#define MAIN_PAGE_WORD_START_Y2 MAIN_PAGE_WORD_START_Y1 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP

//Sub Function Definiton
#define SUB_PAGE_VALUE_START_X  SCREEN_CONTENT_GAP
#define SUB_PAGE_VALUE_START_Y  (SCREEN_Y - SCREEN_LARGE_FONT_SIZE - SCREEN_NORMAL_FONT_SIZE - SCREEN_CONTENT_GAP)/2

#define SUB_PAGE_TIPS_START_X  SCREEN_CONTENT_GAP
#define SUB_PAGE_TIPS_START_Y  SCREEN_Y - SCREEN_NORMAL_FONT_SIZE - SCREEN_CONTENT_GAP

//Sub WIFI Mode & Bind Confirm Definiton
#define SUB_PAGE_ICON_START_X  0
#define SUB_PAGE_ICON_START_Y  (SCREEN_Y -  SCREEN_LARGE_ICON_SIZE)/2

#define SUB_PAGE_WORD_START_X   SUB_PAGE_ICON_START_X + SCREEN_LARGE_ICON_SIZE
#define SUB_PAGE_WORD_START_Y1  (SCREEN_Y -  SCREEN_NORMAL_FONT_SIZE*3 - SCREEN_FONT_GAP*2)/2
#define SUB_PAGE_WORD_START_Y2  SUB_PAGE_WORD_START_Y1 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP
#define SUB_PAGE_WORD_START_Y3  SUB_PAGE_WORD_START_Y2 + SCREEN_NORMAL_FONT_SIZE + SCREEN_FONT_GAP

//Sub Binding Definiton
#define SUB_PAGE_BINDING_WORD_START_X   0
#define SUB_PAGE_BINDING_WORD_START_Y   (SCREEN_Y -  SCREEN_LARGE_FONT_SIZE)/2




