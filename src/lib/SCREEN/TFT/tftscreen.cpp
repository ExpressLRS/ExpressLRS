#ifdef HAS_TFT_SCREEN

#include "tftscreen.h"
#include "options.h"
#include "POWERMGNT.h"

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

bool is_confirmed = false;

const uint16_t *main_menu_icons[] = {
    elrs_rate,
    elrs_power,
    elrs_ratio,
    elrs_motion,
    elrs_fan,
    elrs_bind,
    elrs_updatefw
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

void TFTScreen::init()
{
    tft.init();
    doScreenBackLight(HIGH);

    tft.fillScreen(TFT_WHITE);

    tft.setRotation(1);

    tft.setSwapBytes(true);

    tft.pushImage(0, 0, INIT_PAGE_LOGO_X, INIT_PAGE_LOGO_Y, vendor_logo);

    tft.fillRect(SCREEN_FONT_GAP, INIT_PAGE_FONT_START_Y - INIT_PAGE_FONT_PADDING,
                    SCREEN_X - SCREEN_FONT_GAP*2, SCREEN_NORMAL_FONT_SIZE + INIT_PAGE_FONT_PADDING*2, TFT_BLACK);

    char buffer[50];
    sprintf(buffer, "%s  ELRS-", HARDWARE_VERSION);
    strncat(buffer, thisVersion, 6);
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

void TFTScreen::idleScreen()
{
    tft.fillRect(0, 0, SCREEN_X/2, SCREEN_Y, COLOR_ELRS_BANNER_BACKGROUND);
    tft.fillRect(SCREEN_X/2, 0, SCREEN_X/2, SCREEN_Y, TFT_WHITE);

    tft.pushImage(IDLE_PAGE_START_X, IDLE_PAGE_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_banner);

    char buffer[20];
    strncpy(buffer, thisVersion, 6);
    sprintf(buffer+6, " %02d", system_temperature);
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

void TFTScreen::updateMainMenuPage()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(MAIN_PAGE_ICON_START_X, MAIN_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, main_menu_icons[main_menu_page_index-1]);


    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        main_menu_line_1[main_menu_page_index - 1], TFT_BLACK, TFT_WHITE);

    displayFontCenter(MAIN_PAGE_WORD_START_X, SCREEN_X, MAIN_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        main_menu_line_2[main_menu_page_index - 1], TFT_BLACK, TFT_WHITE);
}

void TFTScreen::updateSubFunctionPage()
{
    tft.fillScreen(TFT_WHITE);

    doValueSelection(USER_ACTION_NONE);

    displayFontCenter(SUB_PAGE_TIPS_START_X, SCREEN_X, SUB_PAGE_TIPS_START_Y,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "PRESS TO CONFIRM", TFT_BLACK, TFT_WHITE);
}

void TFTScreen::updateSubWIFIModePage()
{
    tft.fillScreen(TFT_WHITE);

    tft.pushImage(SUB_PAGE_ICON_START_X, SUB_PAGE_ICON_START_Y, SCREEN_LARGE_ICON_SIZE, SCREEN_LARGE_ICON_SIZE, elrs_wifimode);
#if defined(HOME_WIFI_SSID) && defined(HOME_WIFI_PASSWORD)
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "open http://", TFT_BLACK, TFT_WHITE);

    String host_msg = String(wifi_hostname) + ".local";
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        host_msg, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        "by browser", TFT_BLACK, TFT_WHITE);
#else
    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y1,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        wifi_ap_ssid, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y2,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        wifi_ap_password, TFT_BLACK, TFT_WHITE);

    displayFontCenter(SUB_PAGE_WORD_START_X, SCREEN_X, SUB_PAGE_WORD_START_Y3,  SCREEN_NORMAL_FONT_SIZE, SCREEN_NORMAL_FONT,
                        wifi_ap_address, TFT_BLACK, TFT_WHITE);
#endif
    updatecallback(USER_UPDATE_TYPE_WIFI);
}

void TFTScreen::updateSubBindConfirmPage()
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

void TFTScreen::updateSubBindingPage()
{
    tft.fillScreen(TFT_WHITE);

    displayFontCenter(SUB_PAGE_BINDING_WORD_START_X, SCREEN_X, SUB_PAGE_BINDING_WORD_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        "BINDING", TFT_BLACK, TFT_WHITE);

    updatecallback(USER_UPDATE_TYPE_BINDING);

    current_screen_status = SCREEN_STATUS_BINDING;
}

void TFTScreen::doRateValueSelect(int action)
{
    nextIndex(current_rate_index, action, RATE_MAX_NUMBER);
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        rate_string[current_rate_index], TFT_BLACK, TFT_WHITE);
}

void TFTScreen::doPowerValueSelect(int action)
{
    nextIndex(current_power_index, action, MinPower, MaxPower+1);
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        power_string[current_power_index], TFT_BLACK, TFT_WHITE);
}

void TFTScreen::doRatioValueSelect(int action)
{
    nextIndex(current_ratio_index, action, RATIO_MAX_NUMBER);
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        ratio_string[current_ratio_index], TFT_BLACK, TFT_WHITE);
}


void TFTScreen::doPowerSavingValueSelect(int action)
{
    nextIndex(current_powersaving_index, action, POWERSAVING_MAX_NUMBER);
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        powersaving_string[current_powersaving_index], TFT_BLACK, TFT_WHITE);
}

void TFTScreen::doSmartFanValueSelect(int action)
{
    nextIndex(current_smartfan_index, action, SMARTFAN_MAX_NUMBER);
    displayFontCenter(SUB_PAGE_VALUE_START_X, SCREEN_X, SUB_PAGE_VALUE_START_Y,  SCREEN_LARGE_FONT_SIZE, SCREEN_LARGE_FONT,
                        smartfan_string[current_smartfan_index], TFT_BLACK, TFT_WHITE);
}

void TFTScreen::doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index)
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

void TFTScreen::doTemperatureUpdate(uint8_t temperature)
{
    if(current_screen_status == SCREEN_STATUS_IDLE && system_temperature != temperature)
    {
        char buffer[20];
        strncpy(buffer, thisVersion, 6);
        sprintf(buffer+6, " %02d", temperature);
        displayFontCenterWithCelsius(0, SCREEN_X/2, SCREEN_LARGE_ICON_SIZE + (SCREEN_Y - SCREEN_LARGE_ICON_SIZE - SCREEN_SMALL_FONT_SIZE)/2,
                            SCREEN_SMALL_FONT_SIZE, SCREEN_SMALL_FONT,
                            String(buffer), TFT_WHITE,  COLOR_ELRS_BANNER_BACKGROUND);
    }
    system_temperature = temperature;
}

void TFTScreen::displayFontCenterWithCelsius(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y,
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

void TFTScreen::displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y,
                                            int font_size, int font_type, String font_string,
                                            uint16_t fgColor, uint16_t bgColor)
{
    tft.fillRect(font_start_x, font_start_y, font_end_x - font_start_x, font_size, bgColor);

    int start_pos = font_start_x + (font_end_x - font_start_x -  tft.textWidth(font_string, font_type))/2;
    tft.setCursor(start_pos, font_start_y, font_type);

    tft.setTextColor(fgColor, bgColor);
    tft.print(font_string);
}


void TFTScreen::doScreenBackLight(int state)
{
    #ifdef TFT_BL
    digitalWrite(TFT_BL, state);
    #endif
}
#endif
