#include <functional>
#include <stack>

#include "menu.h"

#include "POWERMGNT.h"
#include "common.h"
#include "config.h"
#include "helpers.h"
#include "logging.h"

#ifdef HAS_TFT_SCREEN
#include "TFT/tftdisplay.h"
extern TFTDisplay screen;
#else
#include "OLED/oleddisplay.h"
extern OLEDDisplay screen;
#endif

#ifdef HAS_THERMAL
#include "thermal.h"
#define UPDATE_TEMP_TIMEOUT 5000
extern Thermal thermal;
#endif

extern bool ICACHE_RAM_ATTR IsArmed();
extern void EnterBindingMode();
extern bool InBindingMode;
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
#endif

typedef enum fsm_action_e
{
    ACTION_GOTO,
    ACTION_NEXT,
    ACTION_PREVIOUS,
    ACTION_PUSH,
    ACTION_POP,
} fsm_action_t;

typedef enum fsm_state_e
{
    STATE_IGNORED = -1,
    STATE_SPLASH = 0,
    STATE_IDLE,
    STATE_PACKET,
    STATE_POWER,
    STATE_TELEMETRY,
    STATE_POWERSAVE,
    STATE_SMARTFAN,
    STATE_BIND,
    STATE_WIFI,

    STATE_VALUE_INIT,
    STATE_VALUE_SELECT,
    STATE_VALUE_INC,
    STATE_VALUE_DEC,
    STATE_VALUE_SAVE,

    STATE_WIFI_CONFIRM,
    STATE_WIFI_EXECUTE,
    STATE_WIFI_STATUS,
    STATE_WIFI_EXIT,

    STATE_BIND_CONFIRM,
    STATE_BIND_EXECUTE,
    STATE_BIND_STATUS,
} fsm_state_t;

typedef struct fsm_state_event_s
{
    fsm_event_t event;
    fsm_action_t action;
    fsm_state_t next;
} fsm_state_event_t;

typedef struct state_entry_s
{
    fsm_state_t state;
    std::function<void()> entry;
    uint32_t timeout;
    fsm_state_event_t const *events;
    uint8_t event_count;
} state_entry_t;

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
    {EVENT_UP, ACTION_GOTO, STATE_BIND},
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

static void displaySplashScreen();
static void displayIdleScreen();
static void displayMenuScreen(menu_item_t menuItem);

// Value menu
static void setupValueIndex();
static void displayValueIndex();
static void incrementValueIndex();
static void decrementValueIndex();
static void saveValueIndex();

// WiFi
static void displayWiFiConfirm();
static void startWiFi();
static void exitWiFi();
static void displayWiFiStatus();

// Bind
static void displayBindConfirm();
static void startBind();
static void displayBindStatus(); // This function will fire a POP transition when if !IsBinding

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

state_entry_t const FSM[] = {
    {STATE_SPLASH, displaySplashScreen, 5000, splash_events, ARRAY_SIZE(splash_events)},
    {STATE_IDLE, displayIdleScreen, 100, idle_events, ARRAY_SIZE(idle_events)},
    {STATE_PACKET, []() { displayMenuScreen(MENU_PACKET); }, 20000, first_menu_events, ARRAY_SIZE(first_menu_events)},
    {STATE_POWER, []() { displayMenuScreen(MENU_POWER); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_TELEMETRY, []() { displayMenuScreen(MENU_TELEMETRY); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#ifdef HAS_THERMAL
    {STATE_POWERSAVE, []() { displayMenuScreen(MENU_POWERSAVE); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#endif
#ifdef HAS_GSENSOR
    {STATE_SMARTFAN, []() { displayMenuScreen(MENU_SMARTFAN); }, 20000, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
#endif
    {STATE_BIND, []() { displayMenuScreen(MENU_BIND); }, 20000, bind_menu_events, ARRAY_SIZE(bind_menu_events)},
    {STATE_WIFI, []() { displayMenuScreen(MENU_WIFI); }, 20000, wifi_menu_events, ARRAY_SIZE(wifi_menu_events)},

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

};

static std::stack<int> state_index_stack;
static int current_state_index;
static uint32_t current_state_entered = 0;
static bool force_pop = false;
static bool idling = false;

void startFSM(uint32_t now)
{
    state_index_stack.push(STATE_IGNORED);
    current_state_index = 0;
    FSM[current_state_index].entry();
    current_state_entered = now;
}

void handleEvent(uint32_t now, fsm_event_t event)
{
    if (force_pop)
    {
        force_pop = false;
        current_state_index = state_index_stack.top();
        state_index_stack.pop();
        FSM[current_state_index].entry();
        current_state_entered = now;
        return handleEvent(now, EVENT_IMMEDIATE);
    }
    // Event timeout has not occurred
    if (event == EVENT_TIMEOUT && now - current_state_entered < FSM[current_state_index].timeout)
    {
        return;
    }
    // scan FSM for matching event in current state
    for (int index = 0 ; index < FSM[current_state_index].event_count ; index++)
    {
        const fsm_state_event_t &action = FSM[current_state_index].events[index];
        if (event == action.event)
        {
            switch (action.action)
            {
                case ACTION_PUSH:
                    state_index_stack.push(current_state_index);
                    // FALL-THROUGH
                case ACTION_GOTO:
                    for (int i = 0 ; i < ARRAY_SIZE(FSM) ; i++)
                    {
                        if (FSM[i].state == action.next)
                        {
                            current_state_index = i;
                            break;
                        }
                    }
                    break;
                case ACTION_POP:
                    current_state_index = state_index_stack.top();
                    state_index_stack.pop();
                    break;
                case ACTION_NEXT:
                    current_state_index++;
                    break;
                case ACTION_PREVIOUS:
                    current_state_index--;
                    break;
            }
            FSM[current_state_index].entry();
            current_state_entered = now;
            return handleEvent(now, EVENT_IMMEDIATE);
        }
    }
}

static void displaySplashScreen()
{
    screen.displaySplashScreen();
}

static void displayIdleScreen()
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
    uint8_t changed = !idling ? 0xFF : 0;
    if (idling)
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

        screen.displayIdleScreen(changed, last_rate, last_power, last_tlm, last_motion, last_fan, last_dynamic, last_run_power, last_temperature, last_message);
    }
    idling = true;
}

static void displayMenuScreen(menu_item_t menuItem)
{
    idling = false;
    screen.displayMainMenu(menuItem);
}

// Value menu
static menu_item_t values_menu;
static int values_min;
static int values_max;
static int values_index;

static void setupValueIndex()
{
    switch (state_index_stack.top())
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

static void displayValueIndex()
{
    screen.displayValue(values_menu, values_index);
}

static void incrementValueIndex()
{
    uint8_t values_count = values_max - values_min + 1;
    values_index = (values_index - values_min + 1) % values_count + values_min;
}

static void decrementValueIndex()
{
    uint8_t values_count = values_max - values_min + 1;
    values_index = (values_index - values_min + values_count - 1) % values_count + values_min;
}

static void saveValueIndex()
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
static void displayWiFiConfirm(){
    screen.displayWiFiConfirm();
}

static void startWiFi()
{
    connectionState = wifiUpdate;
}

static void exitWiFi()
{
#ifdef PLATFORM_ESP32
    if (connectionState == wifiUpdate) {
        rebootTime = millis() + 200;
    }
#endif
}

static void displayWiFiStatus()
{
    screen.displayWiFiStatus();
}

// Bind
static void displayBindConfirm()
{
    screen.displayBindConfirm();
}

static void startBind()
{
    EnterBindingMode();
}

static void displayBindStatus()
{
    if (!InBindingMode)
    {
        force_pop = true;
    }
    else
    {
        screen.displayBindStatus();
    }
}
