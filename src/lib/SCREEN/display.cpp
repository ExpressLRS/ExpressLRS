#include "display.h"
#include "helpers.h"

const char *Display::message_string[] = {
    "ExpressLRS",
    "[  Connected  ]",
    "[  ! Armed !  ]",
    "[  Mismatch!  ]"
};

const char *Display::main_menu_line_1[] = {
    "PACKET",
    "TX",
    "TELEM",
    "MOTION",
    "FAN",
    "BIND",
    "UPDATE"
};

const char *Display::main_menu_line_2[] = {
    "RATE",
    "POWER",
    "RATIO",
    "DETECT",
    "CONTROL",
    "MODE",
    "FW"
};

#if defined(RADIO_SX128X)
const char *rate_string[] = {
    "F1000Hz",
    "F500Hz",
    "500Hz",
    "250Hz",
    "150Hz",
    "50Hz"
};
#else
const char *rate_string[] = {
    "200Hz",
    "100Hz",
    "50Hz",
    "25Hz"
};
#endif

const char *power_string[] = {
    "10mW",
    "25mW",
    "50mW",
    "100mW",
    "250mW",
    "500mW",
    "1000mW",
    "2000mW"
};

const char *ratio_string[] = {
    "Off",
    "1:128",
    "1:64",
    "1:32",
    "1:16",
    "1:8",
    "1:4",
    "1:2"
};

const char *powersaving_string[] = {
    "OFF",
    "ON"
};

const char *smartfan_string[] = {
    "AUTO",
    "ON",
    "OFF"
};

const item_values_t Display::value_sets[] = {
    {rate_string, ARRAY_SIZE(rate_string)},
    {power_string, ARRAY_SIZE(power_string)},
    {ratio_string, ARRAY_SIZE(ratio_string)},
    {powersaving_string, ARRAY_SIZE(powersaving_string)},
    {smartfan_string, ARRAY_SIZE(smartfan_string)}
};