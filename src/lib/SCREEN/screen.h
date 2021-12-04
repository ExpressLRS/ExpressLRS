
#pragma once

#include "targets.h"

class Screen
{
protected:
    // Common Variables
    int current_page_index;
    int main_menu_page_index;
    int current_rate_index;
    int current_power_index;
    int current_ratio_index;
    int current_screen_status;


    virtual void doValueSelection(int action) = 0;
    virtual void doRateValueSelect(int action) = 0;
    virtual void doPowerValueSelect(int action) = 0; 
    virtual void doRatioValueSelect(int action) = 0;
    virtual void doPowerSavingValueSelect(int action) = 0;
    virtual void doSmartFanValueSelect(int action) = 0;

    virtual void updateMainMenuPage(int action) = 0;
    virtual void updateSubFunctionPage(int action) = 0;
    virtual void updateSubWIFIModePage() = 0;
    virtual void updateSubBindConfirmPage() = 0;
    virtual void updateSubBindingPage() = 0;

    virtual void doPageBack() = 0;
    virtual void doPageForward() = 0;
    virtual void doValueConfirm() = 0;        

public:

    virtual void init() = 0;
    virtual void activeScreen() = 0;
    virtual void idleScreen() = 0;
    virtual void doUserAction(int action) = 0;
    virtual void doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index) = 0;

    int getUserRateIndex();
    int getUserPowerIndex();
    int getUserRatioIndex();

};