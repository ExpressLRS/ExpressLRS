#include "menu.h"

#include "POWERMGNT.h"
#include "common.h"
#include "config.h"
#include "helpers.h"
#include "logging.h"

#include "display.h"

#ifdef HAS_THERMAL
#include "thermal.h"
#define UPDATE_TEMP_TIMEOUT 5000
extern Thermal thermal;
#endif

extern FiniteStateMachine state_machine;

extern bool ICACHE_RAM_ATTR IsArmed();
extern void EnterBindingMode();
extern bool InBindingMode;
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
#endif

fsm_state_t getInitialState()
{
#if defined(PLATFORM_ESP32)
    if(esp_reset_reason() == ESP_RST_SW)
    {
        return STATE_IDLE;
    }
#endif
    return STATE_SPLASH;
}

static void displaySplashScreen(bool init)
{
    Display::displaySplashScreen();
}

static void displayIdleScreen(bool init)
{
    static message_index_t last_message = MSG_INVALID;
    static uint8_t last_temperature = 25;
    static uint8_t last_rate = 0xFF;
    static uint8_t last_power = 0xFF;
    static uint8_t last_tlm = 0xFF;
    static uint8_t last_motion = 0xFF;
    static uint8_t last_fan = 0xFF;
    static uint8_t last_dynamic = 0xFF;
    static uint8_t last_run_power = 0xFF;

    uint8_t temperature = last_temperature;
#ifdef HAS_THERMAL
    static uint32_t last_update_temp_ms = 0;
    uint32_t now = millis();
    if(now - last_update_temp_ms > UPDATE_TEMP_TIMEOUT || last_update_temp_ms == 0)
    {
        temperature = thermal.getTempValue();
        last_update_temp_ms = now;
    }
#endif

    message_index_t disp_message = IsArmed() ? MSG_ARMED : ((connectionState == connected) ? (connectionHasModelMatch ? MSG_CONNECTED : MSG_MISMATCH) : MSG_NONE);
    uint8_t changed = init ? 0xFF : 0;
    if (changed == 0)
    {
        changed |= last_message != disp_message ? CHANGED_MESSAGE : 0;
        changed |= last_temperature != temperature ? CHANGED_MESSAGE : 0;
        changed |= last_rate != config.GetRate() ? CHANGED_RATE : 0;
        changed |= last_power != config.GetPower() ? CHANGED_POWER : 0;
        changed |= last_dynamic != config.GetDynamicPower() ? CHANGED_POWER : 0;
        changed |= last_run_power != (uint8_t)(POWERMGNT::currPower()) ? CHANGED_POWER : 0;
        changed |= last_tlm != config.GetTlm() ? CHANGED_TELEMETRY : 0;
        changed |= last_motion != config.GetMotionMode() ? CHANGED_MOTION : 0;
        changed |= last_fan != config.GetFanMode() ? CHANGED_FAN : 0;
    }

    if (changed)
    {
        last_message = disp_message;
        last_temperature = temperature;
        last_rate = config.GetRate();
        last_power = config.GetPower();
        last_tlm = config.GetTlm();
        last_motion = config.GetMotionMode();
        last_fan = config.GetFanMode();
        last_dynamic = config.GetDynamicPower();
        last_run_power = (uint8_t)(POWERMGNT::currPower());

        Display::displayIdleScreen(changed, last_rate, last_power, last_tlm, last_motion, last_fan, last_dynamic, last_run_power, last_temperature, last_message);
    }
}

static void displayMenuScreen(menu_item_t menuItem)
{
    Display::displayMainMenu(menuItem);
}

// Value menu
static menu_item_t values_menu;
static int values_min;
static int values_max;
static int values_index;

static void setupValueIndex(bool init)
{
    switch (state_machine.getParentState())
    {
    case STATE_PACKET:
        values_menu = MENU_PACKET;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetRate();
        break;
    case STATE_TELEMETRY:
        values_menu = MENU_TELEMETRY;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetTlm();
        break;
    case STATE_POWERSAVE:
        values_menu = MENU_POWERSAVE;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetFanMode();
        break;
    case STATE_SMARTFAN:
        values_menu = MENU_SMARTFAN;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetMotionMode();
        break;

    case STATE_POWER_MAX:
        values_menu = MENU_POWER_MAX;
        values_min = MinPower;
        values_max = MaxPower;
        values_index = config.GetPower();
        break;
    case STATE_POWER_DYNAMIC:
        values_menu = MENU_POWER_DYNAMIC;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
        break;

    case STATE_VTX_BAND:
        values_menu = MENU_VTX_BAND;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetVtxBand();
        break;
    case STATE_VTX_CHANNEL:
        values_menu = MENU_VTX_CHANNEL;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetVtxChannel();
        break;
    case STATE_VTX_POWER:
        values_menu = MENU_VTX_POWER;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetVtxPower();
        break;
    case STATE_VTX_PITMODE:
        values_menu = MENU_VTX_PITMODE;
        values_min = 0;
        values_max = Display::getValueCount(values_menu)-1;
        values_index = config.GetVtxPitmode();
        break;
    }
}

static void displayValueIndex(bool init)
{
    Display::displayValue(values_menu, values_index);
}

static void incrementValueIndex(bool init)
{
    uint8_t values_count = values_max - values_min + 1;
    values_index = (values_index - values_min + 1) % values_count + values_min;
}

static void decrementValueIndex(bool init)
{
    uint8_t values_count = values_max - values_min + 1;
    values_index = (values_index - values_min + values_count - 1) % values_count + values_min;
}

static void saveValueIndex(bool init)
{
    switch (values_menu)
    {
        case MENU_PACKET:
            config.SetRate(values_index);
            break;
        case MENU_TELEMETRY:
            config.SetTlm(values_index);
            break;
        case MENU_POWERSAVE:
            config.SetMotionMode(values_index);
            break;
        case MENU_SMARTFAN:
            config.SetFanMode(values_index);
            break;

        case MENU_POWER_MAX:
            config.SetPower(values_index);
            break;
        case MENU_POWER_DYNAMIC:
            config.SetDynamicPower(values_index > 0);
            config.SetBoostChannel((values_index - 1) > 0 ? values_index - 1 : 0);
            break;

        case MENU_VTX_BAND:
            config.SetVtxBand(values_index);
            break;
        case MENU_VTX_CHANNEL:
            config.SetVtxChannel(values_index);
            break;
        case MENU_VTX_POWER:
            config.SetVtxPower(values_index);
            break;
        case MENU_VTX_PITMODE:
            config.SetVtxPitmode(values_index);
            break;
        default:
            break;
    }
}

// WiFi
static void displayWiFiConfirm(bool init)
{
    Display::displayWiFiConfirm();
}

static void startWiFi(bool init)
{
    connectionState = wifiUpdate;
}

static void exitWiFi(bool init)
{
#ifdef PLATFORM_ESP32
    if (connectionState == wifiUpdate) {
        rebootTime = millis() + 200;
    }
#endif
}

static void displayWiFiStatus(bool init)
{
    if (init)
    {
        Display::displayWiFiStatus();
    }
}

// Bind
static void displayBindConfirm(bool init)
{
    Display::displayBindConfirm();
}

static void startBind(bool init)
{
    EnterBindingMode();
}

static void displayBindStatus(bool init)
{
    if (!InBindingMode)
    {
        state_machine.popState();
    }
    else
    {
        if (init)
        {
            Display::displayBindStatus();
        }
    }
}

//-------------------------------------------------------------------

// Value submenu FSM
fsm_state_event_t const value_init_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_select_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_VALUE_SAVE)},
    {EVENT_RIGHT, GOTO(STATE_VALUE_SAVE)},
    {EVENT_UP, GOTO(STATE_VALUE_DEC)},
    {EVENT_DOWN, GOTO(STATE_VALUE_INC)}
};
fsm_state_event_t const value_increment_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_decrement_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_save_events[] = {{EVENT_IMMEDIATE, ACTION_POP}};

fsm_state_entry_t const value_select_fsm[] = {
    {STATE_VALUE_INIT, setupValueIndex, 0, value_init_events, ARRAY_SIZE(value_init_events)},
    {STATE_VALUE_SELECT, displayValueIndex, 20000, value_select_events, ARRAY_SIZE(value_select_events)},
    {STATE_VALUE_INC, incrementValueIndex, 0, value_increment_events, ARRAY_SIZE(value_increment_events)},
    {STATE_VALUE_DEC, decrementValueIndex, 0, value_decrement_events, ARRAY_SIZE(value_decrement_events)},
    {STATE_VALUE_SAVE, saveValueIndex, 0, value_save_events, ARRAY_SIZE(value_save_events)},
    {STATE_LAST}
};

// Power FSM
fsm_state_event_t const power_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT},
    {EVENT_ENTER, ACTION_PUSH, value_select_fsm},
    {EVENT_RIGHT, ACTION_PUSH, value_select_fsm}
};
fsm_state_entry_t const power_menu_fsm[] = {
    {STATE_POWER_MAX, [](bool init) { displayMenuScreen(MENU_POWER_MAX); }, 20000, power_events, ARRAY_SIZE(power_events)},
    {STATE_POWER_DYNAMIC, [](bool init) { displayMenuScreen(MENU_POWER_DYNAMIC); }, 20000, power_events, ARRAY_SIZE(power_events)},
    {STATE_LAST}
};

// VTX Admin FSM
fsm_state_event_t const vtx_admin_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT},
    {EVENT_ENTER, ACTION_PUSH, value_select_fsm},
    {EVENT_RIGHT, ACTION_PUSH, value_select_fsm}
};
fsm_state_entry_t const vtx_menu_fsm[] = {
    {STATE_VTX_BAND, [](bool init) { displayMenuScreen(MENU_VTX_BAND); }, 20000, vtx_admin_events, ARRAY_SIZE(vtx_admin_events)},
    {STATE_VTX_CHANNEL, [](bool init) { displayMenuScreen(MENU_VTX_CHANNEL); }, 20000, vtx_admin_events, ARRAY_SIZE(vtx_admin_events)},
    {STATE_VTX_POWER, [](bool init) { displayMenuScreen(MENU_VTX_POWER); }, 20000, vtx_admin_events, ARRAY_SIZE(vtx_admin_events)},
    {STATE_VTX_PITMODE, [](bool init) { displayMenuScreen(MENU_VTX_PITMODE); }, 20000, vtx_admin_events, ARRAY_SIZE(vtx_admin_events)},
    {STATE_LAST}
};

// WiFi Update FSM
fsm_state_event_t const wifi_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_WIFI_EXECUTE)},
    {EVENT_RIGHT, GOTO(STATE_WIFI_EXECUTE)}
};
fsm_state_event_t const wifi_execute_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_WIFI_STATUS)}};
fsm_state_event_t const wifi_status_events[] = {{EVENT_TIMEOUT, GOTO(STATE_WIFI_STATUS)}, {EVENT_LEFT, GOTO(STATE_WIFI_EXIT)}};
fsm_state_event_t const wifi_exit_events[] = {{EVENT_IMMEDIATE, ACTION_POP}};

fsm_state_entry_t const wifi_menu_fsm[] = {
    {STATE_WIFI_CONFIRM, displayWiFiConfirm, 20000, wifi_confirm_events, ARRAY_SIZE(wifi_confirm_events)},
    {STATE_WIFI_EXECUTE, startWiFi, 0, wifi_execute_events, ARRAY_SIZE(wifi_execute_events)},
    {STATE_WIFI_STATUS, displayWiFiStatus, 1000, wifi_status_events, ARRAY_SIZE(wifi_status_events)},
    {STATE_WIFI_EXIT, exitWiFi, 0, wifi_exit_events, ARRAY_SIZE(wifi_exit_events)},
    {STATE_LAST}
};

// Bind FSM
fsm_state_event_t const bind_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_BIND_EXECUTE)},
    {EVENT_RIGHT, GOTO(STATE_BIND_EXECUTE)}
};
fsm_state_event_t const bind_execute_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_BIND_STATUS)}};
fsm_state_event_t const bind_status_events[] = {{EVENT_TIMEOUT, GOTO(STATE_BIND_STATUS)}};

fsm_state_entry_t const bind_menu_fsm[] = {
    {STATE_BIND_CONFIRM, displayBindConfirm, 20000, bind_confirm_events, ARRAY_SIZE(bind_confirm_events)},
    {STATE_BIND_EXECUTE, startBind, 0, bind_execute_events, ARRAY_SIZE(bind_execute_events)},
    {STATE_BIND_STATUS, displayBindStatus, 1000, bind_status_events, ARRAY_SIZE(bind_status_events)},
    {STATE_LAST}
};

// Main menu FSM
fsm_state_event_t const value_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, ACTION_PUSH, value_select_fsm},
    {EVENT_RIGHT, ACTION_PUSH, value_select_fsm},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT}
};
fsm_state_event_t const power_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, ACTION_PUSH, power_menu_fsm},
    {EVENT_RIGHT, ACTION_PUSH, power_menu_fsm},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT}
};
fsm_state_event_t const vtx_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, ACTION_PUSH, vtx_menu_fsm},
    {EVENT_RIGHT, ACTION_PUSH, vtx_menu_fsm},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT}
};
fsm_state_event_t const bind_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, ACTION_PUSH, bind_menu_fsm},
    {EVENT_RIGHT, ACTION_PUSH, bind_menu_fsm},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT}
};
fsm_state_event_t const wifi_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_POP},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, ACTION_PUSH, wifi_menu_fsm},
    {EVENT_RIGHT, ACTION_PUSH, wifi_menu_fsm},
    {EVENT_UP, ACTION_PREVIOUS},
    {EVENT_DOWN, ACTION_NEXT}
};

fsm_state_entry_t const main_menu_fsm[] = {
    {STATE_PACKET, [](bool init) { displayMenuScreen(MENU_PACKET); }, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_POWER, [](bool init) { displayMenuScreen(MENU_POWER); }, 20000, power_menu_events, ARRAY_SIZE(power_menu_events)},
    {STATE_TELEMETRY, [](bool init) { displayMenuScreen(MENU_TELEMETRY); }, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#ifdef HAS_THERMAL
    {STATE_POWERSAVE, [](bool init) { displayMenuScreen(MENU_POWERSAVE); }, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#endif
#ifdef HAS_GSENSOR
    {STATE_SMARTFAN, [](bool init) { displayMenuScreen(MENU_SMARTFAN); }, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#endif
    {STATE_VTX, [](bool init) { displayMenuScreen(MENU_VTX); }, 20000, vtx_menu_events, ARRAY_SIZE(vtx_menu_events)},
    {STATE_BIND, [](bool init) { displayMenuScreen(MENU_BIND); }, 20000, bind_menu_events, ARRAY_SIZE(bind_menu_events)},
    {STATE_WIFI, [](bool init) { displayMenuScreen(MENU_WIFI); }, 20000, wifi_menu_events, ARRAY_SIZE(wifi_menu_events)},
    {STATE_LAST}
};

// Entry FSM
fsm_state_event_t const splash_events[] = {
    {EVENT_TIMEOUT, GOTO(STATE_IDLE)}
};
fsm_state_event_t const idle_events[] = {
    {EVENT_TIMEOUT, GOTO(STATE_IDLE)},
    {EVENT_LONG_ENTER, ACTION_PUSH, main_menu_fsm},
    {EVENT_LONG_RIGHT, ACTION_PUSH, main_menu_fsm}
};
fsm_state_entry_t const entry_fsm[] = {
    {STATE_SPLASH, displaySplashScreen, 5000, splash_events, ARRAY_SIZE(splash_events)},
    {STATE_IDLE, displayIdleScreen, 100, idle_events, ARRAY_SIZE(idle_events)},
    {STATE_LAST}
};
