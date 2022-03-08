#include "display.h"
#include "helpers.h"

const char *Display::message_string[] = {
    "ExpressLRS",
    "[  Connected  ]",
    "[  ! Armed !  ]",
    "[  Mismatch!  ]"
};

const char *Display::main_menu_line_1[_MENU_COUNT_] = {
    "PACKET",
    "TX",
    "TELEM",
    "MOTION",
    "FAN",
    "VTX",
    "BIND",
    "UPDATE",

    "MAX",
    "DYNAMIC",

    "VTX",
    "VTX",
    "VTX",
    "VTX"
};

const char *Display::main_menu_line_2[_MENU_COUNT_] = {
    "RATE",
    "POWER",
    "RATIO",
    "DETECT",
    "CONTROL",
    "ADMIN",
    "MODE",
    "FW",

    "POWER",
    "POWER",

    "BAND",
    "CHANNEL",
    "POWER",
    "PITMODE"
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

const char *dynamic_string[] = {
    "OFF",
    "ON",
    "AUX9",
    "AUX10",
    "AUX11",
    "AUX12"
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

const char *band_string[] = {
    "OFF",
    "A",
    "B",
    "E",
    "F",
    "R",
    "L"
};

const char *channel_string[] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8"
};

const char *vtx_power_string[] = {
    "-",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8"
};

const char *pitmode_string[] = {
    "OFF", "ON",
    "+AUX1", "-AXU1",
    "+AUX2", "-AXU2",
    "+AUX3", "-AXU3",
    "+AUX4", "-AXU4",
    "+AUX5", "-AXU5",
    "+AUX6", "-AXU6",
    "+AUX7", "-AXU7",
    "+AUX8", "-AXU8",
    "+AUX9", "-AXU9",
    "+AUX10", "-AXU10"
};

int Display::getValueCount(menu_item_t menu)
{
    switch (menu)
    {
    case MENU_PACKET:
        return ARRAY_SIZE(rate_string);
    case MENU_TELEMETRY:
        return ARRAY_SIZE(ratio_string);
    case MENU_POWERSAVE:
        return ARRAY_SIZE(powersaving_string);
    case MENU_SMARTFAN:
        return ARRAY_SIZE(smartfan_string);

    case MENU_POWER:
    case MENU_POWER_MAX:
        return ARRAY_SIZE(power_string);
    case MENU_POWER_DYNAMIC:
        return ARRAY_SIZE(dynamic_string);

    case MENU_VTX_BAND:
        return ARRAY_SIZE(band_string);
    case MENU_VTX_CHANNEL:
        return ARRAY_SIZE(channel_string);
    case MENU_VTX_POWER:
        return ARRAY_SIZE(vtx_power_string);
    case MENU_VTX_PITMODE:
        return ARRAY_SIZE(pitmode_string);
    default:
        return 0;
    }
}

const char *Display::getValue(menu_item_t menu, uint8_t value_index)
{
    switch (menu)
    {
    case MENU_PACKET:
        return rate_string[value_index];
    case MENU_TELEMETRY:
        return ratio_string[value_index];
    case MENU_POWERSAVE:
        return powersaving_string[value_index];
    case MENU_SMARTFAN:
        return smartfan_string[value_index];

    case MENU_POWER:
    case MENU_POWER_MAX:
        return power_string[value_index];
    case MENU_POWER_DYNAMIC:
        return dynamic_string[value_index];

    case MENU_VTX_BAND:
        return band_string[value_index];
    case MENU_VTX_CHANNEL:
        return channel_string[value_index];
    case MENU_VTX_POWER:
        return vtx_power_string[value_index];
    case MENU_VTX_PITMODE:
        return pitmode_string[value_index];
    default:
        return nullptr;
    }
}
