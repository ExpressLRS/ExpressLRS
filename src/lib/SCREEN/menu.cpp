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

static void displaySplashScreen(bool init)
{
    Display::displaySplashScreen();
}

static void displayIdleScreen(bool init)
{
    static message_index_t last_message = MSG_INVALID;
    static uint8_t last_temperature = 0xFF;
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
#else
    temperature = 25;
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
        values_max = Display::value_sets[MENU_PACKET].count-1;
        values_index = config.GetRate();
        break;
    case STATE_POWER:
        values_menu = MENU_POWER;
        values_min = MinPower;
        values_max = MaxPower;
        values_index = config.GetPower();
        break;
    case STATE_TELEMETRY:
        values_menu = MENU_TELEMETRY;
        values_min = 0;
        values_max = Display::value_sets[MENU_TELEMETRY].count-1;
        values_index = config.GetTlm();
        break;
    case STATE_POWERSAVE:
        values_menu = MENU_POWERSAVE;
        values_min = 0;
        values_max = Display::value_sets[MENU_POWERSAVE].count-1;
        values_index = config.GetFanMode();
        break;
    case STATE_SMARTFAN:
        values_menu = MENU_SMARTFAN;
        values_min = 0;
        values_max = Display::value_sets[MENU_SMARTFAN].count-1;
        values_index = config.GetMotionMode();
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
        case MENU_POWER:
            config.SetPower(values_index);
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

//---------------------------------------------------------


fsm_state_event_t const splash_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const idle_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_LONG_ENTER, ACTION_GOTO, STATE_PACKET},
    {EVENT_LONG_RIGHT, ACTION_GOTO, STATE_PACKET}
};
fsm_state_event_t const first_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_VALUE_INIT},
    {EVENT_RIGHT, ACTION_PUSH, STATE_VALUE_INIT},
    {EVENT_UP, ACTION_GOTO, STATE_WIFI},
    {EVENT_DOWN, ACTION_NEXT, STATE_IGNORED},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const middle_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_VALUE_INIT},
    {EVENT_RIGHT, ACTION_PUSH, STATE_VALUE_INIT},
    {EVENT_UP, ACTION_PREVIOUS, STATE_IGNORED},
    {EVENT_DOWN, ACTION_NEXT, STATE_IGNORED},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const bind_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_BIND_CONFIRM},
    {EVENT_RIGHT, ACTION_PUSH, STATE_BIND_CONFIRM},
    {EVENT_UP, ACTION_PREVIOUS, STATE_IGNORED},
    {EVENT_DOWN, ACTION_NEXT, STATE_IGNORED},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const wifi_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_WIFI_CONFIRM},
    {EVENT_RIGHT, ACTION_PUSH, STATE_WIFI_CONFIRM},
    {EVENT_UP, ACTION_PREVIOUS, STATE_IGNORED},
    {EVENT_DOWN, ACTION_GOTO, STATE_PACKET},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};

// Value submenu
fsm_state_event_t const value_init_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_select_events[] = {
    {EVENT_TIMEOUT, ACTION_POP, STATE_IGNORED},
    {EVENT_LEFT, ACTION_POP, STATE_IGNORED},
    {EVENT_ENTER, ACTION_GOTO, STATE_VALUE_SAVE},
    {EVENT_RIGHT, ACTION_GOTO, STATE_VALUE_SAVE},
    {EVENT_UP, ACTION_GOTO, STATE_VALUE_DEC},
    {EVENT_DOWN, ACTION_GOTO, STATE_VALUE_INC},
};
fsm_state_event_t const value_increment_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_decrement_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_save_events[] = {{EVENT_IMMEDIATE, ACTION_POP, STATE_IGNORED}};

// WiFi submenu
fsm_state_event_t const wifi_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POP, STATE_IGNORED},
    {EVENT_LEFT, ACTION_POP, STATE_IGNORED},
    {EVENT_ENTER, ACTION_GOTO, STATE_WIFI_EXECUTE},
    {EVENT_RIGHT, ACTION_GOTO, STATE_WIFI_EXECUTE}
};
fsm_state_event_t const wifi_execute_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_WIFI_STATUS}};
fsm_state_event_t const wifi_status_events[] = {{EVENT_TIMEOUT, ACTION_GOTO, STATE_WIFI_STATUS}, {EVENT_LEFT, ACTION_GOTO, STATE_WIFI_EXIT}};
fsm_state_event_t const wifi_exit_events[] = {{EVENT_IMMEDIATE, ACTION_POP, STATE_IGNORED}};

// Bind submenu
fsm_state_event_t const bind_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POP, STATE_IGNORED},
    {EVENT_LEFT, ACTION_POP, STATE_IGNORED},
    {EVENT_ENTER, ACTION_GOTO, STATE_BIND_EXECUTE},
    {EVENT_RIGHT, ACTION_GOTO, STATE_BIND_EXECUTE}
};
fsm_state_event_t const bind_execute_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_BIND_STATUS}};
fsm_state_event_t const bind_status_events[] = {{EVENT_TIMEOUT, ACTION_GOTO, STATE_BIND_STATUS}};

fsm_state_entry_t const menu_fsm[] = {
    {STATE_SPLASH, displaySplashScreen, 5000, splash_events, ARRAY_SIZE(splash_events)},
    {STATE_IDLE, displayIdleScreen, 100, idle_events, ARRAY_SIZE(idle_events)},
    {STATE_PACKET, [](bool init) { displayMenuScreen(MENU_PACKET); }, 20000, first_menu_events, ARRAY_SIZE(first_menu_events)},
    {STATE_POWER, [](bool init) { displayMenuScreen(MENU_POWER); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_TELEMETRY, [](bool init) { displayMenuScreen(MENU_TELEMETRY); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#ifdef HAS_THERMAL
    {STATE_POWERSAVE, [](bool init) { displayMenuScreen(MENU_POWERSAVE); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#endif
#ifdef HAS_GSENSOR
    {STATE_SMARTFAN, [](bool init) { displayMenuScreen(MENU_SMARTFAN); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#endif
    {STATE_BIND, [](bool init) { displayMenuScreen(MENU_BIND); }, 20000, bind_menu_events, ARRAY_SIZE(bind_menu_events)},
    {STATE_WIFI, [](bool init) { displayMenuScreen(MENU_WIFI); }, 20000, wifi_menu_events, ARRAY_SIZE(wifi_menu_events)},

    // Value submenu
    {STATE_VALUE_INIT, setupValueIndex, 0, value_init_events, ARRAY_SIZE(value_init_events)},
    {STATE_VALUE_SELECT, displayValueIndex, 20000, value_select_events, ARRAY_SIZE(value_select_events)},
    {STATE_VALUE_INC, incrementValueIndex, 0, value_increment_events, ARRAY_SIZE(value_increment_events)},
    {STATE_VALUE_DEC, decrementValueIndex, 0, value_decrement_events, ARRAY_SIZE(value_decrement_events)},
    {STATE_VALUE_SAVE, saveValueIndex, 0, value_save_events, ARRAY_SIZE(value_save_events)},

    // WiFi submenu
    {STATE_WIFI_CONFIRM, displayWiFiConfirm, 20000, wifi_confirm_events, ARRAY_SIZE(wifi_confirm_events)},
    {STATE_WIFI_EXECUTE, startWiFi, 0, wifi_execute_events, ARRAY_SIZE(wifi_execute_events)},
    {STATE_WIFI_STATUS, displayWiFiStatus, 1000, wifi_status_events, ARRAY_SIZE(wifi_status_events)},
    {STATE_WIFI_EXIT, exitWiFi, 0, wifi_exit_events, ARRAY_SIZE(wifi_exit_events)},

    // Bind submenu
    {STATE_BIND_CONFIRM, displayBindConfirm, 20000, bind_confirm_events, ARRAY_SIZE(bind_confirm_events)},
    {STATE_BIND_EXECUTE, startBind, 0, bind_execute_events, ARRAY_SIZE(bind_execute_events)},
    {STATE_BIND_STATUS, displayBindStatus, 1000, bind_status_events, ARRAY_SIZE(bind_status_events)},

    {STATE_IGNORED}
};
