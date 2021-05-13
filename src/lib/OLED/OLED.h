#pragma once
#include "targets.h"
#include <U8g2lib.h>
#include "POWERMGNT.h"
#include "common.h"


class OLED
{

private:


public:
    static void displayLogo();
    static void updateScreen(PowerLevels_e power, expresslrs_RFrates_e rate, expresslrs_tlm_ratio_e ratio);

};