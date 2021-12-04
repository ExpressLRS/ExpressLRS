#include "screen.h"
#include "FiveWayButton.h"


int Screen::getUserRateIndex()
{
    return current_rate_index;
}

int Screen::getUserPowerIndex()
{
    return current_power_index;
}

int Screen::getUserRatioIndex()
{
    return current_ratio_index;
}

int Screen::getScreenStatus()
{
    return current_screen_status;
}