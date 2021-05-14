#pragma once
#include "targets.h"
#include <U8g2lib.h>
#include "POWERMGNT.h"


class OLED
{

private:

public:
    static void displayLogo();
    static void updateScreen(const char * rate, const char * ratio, const char * power, const char * commit);
};