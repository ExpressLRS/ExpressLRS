#pragma once
#include "targets.h"
#include <U8g2lib.h>
#include "POWERMGNT.h"
#include "common.h"


class OLED
{

private:

public:
    char * getTLMRatioString(expresslrs_tlm_ratio_e ratio);
    const char * getRateString(expresslrs_RFrates_e rate);
    const char * getPowerString(PowerLevels_e power);
    static void displayLogo();
    static void updateScreen(const char * rate, const char * ratio, const char * power);

};