#pragma once
#include "targets.h"
#include <U8g2lib.h>
#include "POWERMGNT.h"


class OLED
{

private:

public:
    static void displayLogo();
    const char * getPowerString(int power);
    const char * getRateString(int rate);
    const char * getTLMRatioString(int ratio);
    static void updateScreen(const char * power, const char * rate, const char * ratio, const char * commit);
};