#include "display.h"
#include "helpers.h"

extern bool OtaIsFullRes;

const char *Display::message_string[] = {
    "ExpressLRS",
    "[  Connected  ]",
    "[  ! Armed !  ]",
    "[  Mismatch!  ]"
};

const char *Display::main_menu_strings[][2] = {
    {"PACKET", "RATE"},
    {"SWITCH", "MODE"},
    {"ANTENNA", "MODE"},
    {"TX", "POWER"},
    {"TELEM", "RATIO"},
    {"MOTION", "DETECT"},
    {"FAN", "CONTROL"},
    {"BLE", "GAMEPAD"},
    {"BIND", "MODE"},
    {"WIFI", "ADMIN"},
    {"VTX", "ADMIN"},

    {"MAX", "POWER"},
    {"DYNAMIC", "POWER"},

    {"VTX", "BAND"},
    {"VTX", "CHANNEL"},
    {"VTX", "POWER"},
    {"VTX", "PITMODE"},
    {"VTX", "SEND"},

    {"TX", "WIFI"},
    {"RX", "WIFI"},
    {"BACKPAC", "WIFI"},
    {"VRX", "WIFI"},
};

#if defined(RADIO_SX128X)
const char *rate_string[] = {
    "F1000Hz",
    "F500Hz",
    "D500Hz",
    "D250Hz",
    "500Hz",
    "333 Full",
    "250Hz",
    "150Hz",
    "100 Full",
    "50Hz"
};
#elif defined(RADIO_LR1121)
static const char *rate_string[] = {
    // Dual
    "X150Hz",
    "X100 Full",
    // 900
    "250Hz",
    "200 Full",
    "200Hz",
    "100 Full",
    "100Hz",
    "50Hz",
    // 2.4
    "500Hz",
    "333 Full",
    "250Hz",
    "150Hz",
    "100 Full",
    "50Hz"
};
#else
static const char *rate_string[] = {
    "200Hz",
    "100 Full",
    "100Hz",
    "50Hz",
    "25Hz",
    "D50Hz"
};
#endif

static const char *switch_mode[] = {
    "Wide",
    "Hybrid",
};

static const char *switch_mode_full[] = {
    "8Ch",
    "16Ch /2",
    "12Ch Mix"
};

static const char *antenna_mode[] = {
    "Gemini",
    "Ant 1",
    "Ant 2",
    "Switch",
};

static const char *power_string[] = {
    "10mW",
    "25mW",
    "50mW",
    "100mW",
    "250mW",
    "500mW",
    "1000mW",
    "2000mW"
};

static const char *dynamic_string[] = {
    "OFF",
    "ON",
    "AUX9",
    "AUX10",
    "AUX11",
    "AUX12"
};

static const char *ratio_string[] = {
    "Std",
    "Off",
    "1:128",
    "1:64",
    "1:32",
    "1:16",
    "1:8",
    "1:4",
    "1:2",
    "Race"
};

static const char *ratio_curr_string[] = {
    "Off",
    "1:2",
    "1:4",
    "1:8",
    "1:16",
    "1:32",
    "1:64",
    "1:128"
};

static const char *powersaving_string[] = {
    "OFF",
    "ON"
};

static const char *smartfan_string[] = {
    "AUTO",
    "ON",
    "OFF"
};

static const char *band_string[] = {
    "OFF",
    "A",
    "B",
    "E",
    "F",
    "R",
    "L"
};

static const char *channel_string[] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8"
};

static const char *vtx_power_string[] = {
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

static const char *pitmode_string[] = {
    "OFF", "ON",
    "AUX1 !+", "AUX1 !-",
    "AUX2 !+", "AUX2 !-",
    "AUX3 !+", "AUX3 !-",
    "AUX4 !+", "AUX4 !-",
    "AUX5 !+", "AUX5 !-",
    "AUX6 !+", "AUX6 !-",
    "AUX7 !+", "AUX7 !-",
    "AUX8 !+", "AUX8 !-",
    "AUX9 !+", "AUX9 !-",
    "AUX10 !+", "AUX10 !-"
};

int Display::getValueCount(menu_item_t menu)
{
    switch (menu)
    {
    case STATE_PACKET:
        return ARRAY_SIZE(rate_string);
    case STATE_SWITCH:
        if (OtaIsFullRes)
        {
            return ARRAY_SIZE(switch_mode_full);
        }
        return ARRAY_SIZE(switch_mode);
    case STATE_ANTENNA:
        return ARRAY_SIZE(antenna_mode);
    case STATE_TELEMETRY:
        return ARRAY_SIZE(ratio_string);
    case STATE_POWERSAVE:
        return ARRAY_SIZE(powersaving_string);
    case STATE_SMARTFAN:
        return ARRAY_SIZE(smartfan_string);

    case STATE_POWER:
    case STATE_POWER_MAX:
        return ARRAY_SIZE(power_string);
    case STATE_POWER_DYNAMIC:
        return ARRAY_SIZE(dynamic_string);

    case STATE_VTX_BAND:
        return ARRAY_SIZE(band_string);
    case STATE_VTX_CHANNEL:
        return ARRAY_SIZE(channel_string);
    case STATE_VTX_POWER:
        return ARRAY_SIZE(vtx_power_string);
    case STATE_VTX_PITMODE:
        return ARRAY_SIZE(pitmode_string);
    default:
        return 0;
    }
}

const char *Display::getValue(menu_item_t menu, uint8_t value_index)
{
    switch (menu)
    {
    case STATE_PACKET:
#if defined(RADIO_LR1121) // Janky fix to order menu correctly.  This is also untested on a display with a joystick.
        value_index = (value_index + 4) % 14; // 14 = RATE_MAX
#endif
        return rate_string[value_index];
    case STATE_SWITCH:
        if (OtaIsFullRes)
        {
            return switch_mode_full[value_index];
        }
        return switch_mode[value_index];
    case STATE_ANTENNA:
        return antenna_mode[value_index];
    case STATE_TELEMETRY:
        return ratio_string[value_index];
    case STATE_TELEMETRY_CURR:
        return ratio_curr_string[value_index];
    case STATE_POWERSAVE:
        return powersaving_string[value_index];
    case STATE_SMARTFAN:
        return smartfan_string[value_index];

    case STATE_POWER:
    case STATE_POWER_MAX:
        return power_string[value_index];
    case STATE_POWER_DYNAMIC:
        return dynamic_string[value_index];

    case STATE_VTX_BAND:
        return band_string[value_index];
    case STATE_VTX_CHANNEL:
        return channel_string[value_index];
    case STATE_VTX_POWER:
        return vtx_power_string[value_index];
    case STATE_VTX_PITMODE:
        return pitmode_string[value_index];
    default:
        return nullptr;
    }
}
