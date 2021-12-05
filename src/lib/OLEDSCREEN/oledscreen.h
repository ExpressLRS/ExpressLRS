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

class OLEDScreen: public Screen
{
private:
    void doRateValueSelect(int action);
    void doPowerValueSelect(int action);
    void doRatioValueSelect(int action);
    void doPowerSavingValueSelect(int action);
    void doSmartFanValueSelect(int action);

    void updateMainMenuPage();
    void updateSubFunctionPage();
    void updateSubWIFIModePage();
    void updateSubBindConfirmPage();
    void updateSubBindingPage();

    void doPageBack();
    void doPageForward();
    void doValueConfirm();

    void displayMainScreen();

public:

    void init();
    void idleScreen();
    void doParamUpdate(uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index);
    void doTemperatureUpdate(uint8_t temperature);
    void doScreenBackLight(int state);
};