#include <functional>
#include <stack>

#include "menu.h"
#include "common.h"
#include "config.h"

#include "helpers.h"

#ifdef HAS_TFT_SCREEN
#include "TFT/tftscreen.h"
extern TFTScreen screen;
#else
#include "OLED/oledscreen.h"
extern OLEDScreen screen;
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
    STATE_WIFI,
    STATE_BIND,

    STATE_VALUE_INIT,
    STATE_VALUE_SELECT,
    STATE_VALUE_INC,
    STATE_VALUE_DEC,
    STATE_VALUE_SAVE,

    STATE_WIFI_EXECUTE,
    STATE_WIFI_STATUS,
    STATE_WIFI_EXIT,
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
    uint8_t timeout;
    fsm_state_event_t const *events;
    uint8_t event_count;
} state_entry_t;

fsm_state_event_t const splash_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const idle_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_GOTO, STATE_PACKET},
    {EVENT_RIGHT, ACTION_GOTO, STATE_PACKET}
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
fsm_state_event_t const wifi_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_WIFI_EXECUTE},
    {EVENT_RIGHT, ACTION_PUSH, STATE_WIFI_EXECUTE},
    {EVENT_UP, ACTION_PREVIOUS, STATE_IGNORED},
    {EVENT_DOWN, ACTION_NEXT, STATE_IGNORED},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};
fsm_state_event_t const bind_menu_events[] = {
    {EVENT_TIMEOUT, ACTION_GOTO, STATE_IDLE},
    {EVENT_ENTER, ACTION_PUSH, STATE_BIND_EXECUTE},
    {EVENT_RIGHT, ACTION_PUSH, STATE_BIND_EXECUTE},
    {EVENT_UP, ACTION_PREVIOUS, STATE_IGNORED},
    {EVENT_DOWN, ACTION_GOTO, STATE_PACKET},
    {EVENT_LEFT, ACTION_GOTO, STATE_IDLE}
};

#define MENU_PACKET 0
#define MENU_POWER 1
#define MENU_TELEMETRY 2
#define MENU_POWERSAVE 3
#define MENU_SMARTFAN 4
#define MENU_WIFI 5
#define MENU_BIND 6
static void displaySplashScreen();
static void displayIdleScreen();
static void displayMenuScreen(int menuItem);

// Value menu
static void setupValueIndex();
static void displayValueIndex();
static void incrementValueIndex();
static void decrementValueIndex();
static void saveValueIndex();

// WiFi
static void startWiFi();
static void exitWiFi();
static void displayWiFiStatus();

// Bind
static void startBind();
static void displayBindStatus(); // This function will fire a POP transition when if !IsBinding

// Value submenu
fsm_state_event_t const value_init_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_select_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_increment_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_decrement_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_VALUE_SELECT}};
fsm_state_event_t const value_save_events[] = {{EVENT_IMMEDIATE, ACTION_POP, STATE_IGNORED}};

// WiFi submenu
fsm_state_event_t const wifi_execute_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_WIFI_STATUS}};
fsm_state_event_t const wifi_status_events[] = {{EVENT_TIMEOUT, ACTION_GOTO, STATE_WIFI_STATUS}, {EVENT_LEFT, ACTION_GOTO, STATE_WIFI_EXIT}};
fsm_state_event_t const wifi_exit_events[] = {{EVENT_IMMEDIATE, ACTION_POP, STATE_IGNORED}};

// Bind submenu
fsm_state_event_t const bind_execute_events[] = {{EVENT_IMMEDIATE, ACTION_GOTO, STATE_BIND_STATUS}};
fsm_state_event_t const bind_status_events[] = {{EVENT_TIMEOUT, ACTION_GOTO, STATE_BIND_STATUS}};

state_entry_t const FSM[] = {
    {STATE_SPLASH, displaySplashScreen, 5, splash_events, ARRAY_SIZE(splash_events)},
    {STATE_IDLE, displayIdleScreen, 1, idle_events, ARRAY_SIZE(idle_events)},
    {STATE_PACKET, []() { displayMenuScreen(MENU_PACKET); }, 1, first_menu_events, ARRAY_SIZE(first_menu_events)},
    {STATE_POWER, []() { displayMenuScreen(MENU_POWER); }, 1, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_TELEMETRY, []() { displayMenuScreen(MENU_TELEMETRY); }, 1, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_POWERSAVE, []() { displayMenuScreen(MENU_POWERSAVE); }, 1, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_SMARTFAN, []() { displayMenuScreen(MENU_SMARTFAN); }, 1, middle_menu_events, ARRAY_SIZE(middle_menu_events)},
    {STATE_WIFI, []() { displayMenuScreen(MENU_WIFI); }, 1, wifi_menu_events, ARRAY_SIZE(wifi_menu_events)},
    {STATE_BIND, []() { displayMenuScreen(MENU_BIND); }, 1, bind_menu_events, ARRAY_SIZE(bind_menu_events)},

    // Value submenu
    {STATE_VALUE_INIT, setupValueIndex, 0, value_init_events, ARRAY_SIZE(value_init_events)},
    {STATE_VALUE_SELECT, displayValueIndex, 10, value_select_events, ARRAY_SIZE(value_select_events)},
    {STATE_VALUE_INC, incrementValueIndex, 10, value_increment_events, ARRAY_SIZE(value_increment_events)},
    {STATE_VALUE_DEC, decrementValueIndex, 10, value_decrement_events, ARRAY_SIZE(value_decrement_events)},
    {STATE_VALUE_SAVE, saveValueIndex, 10, value_save_events, ARRAY_SIZE(value_save_events)},

    // WiFi submenu
    {STATE_WIFI_EXECUTE, startWiFi, 0, wifi_execute_events, ARRAY_SIZE(wifi_execute_events)},
    {STATE_WIFI_STATUS, displayWiFiStatus, 1, wifi_status_events, ARRAY_SIZE(wifi_status_events)},
    {STATE_WIFI_EXIT, exitWiFi, 0, wifi_exit_events, ARRAY_SIZE(wifi_exit_events)},

    // Bind submenu
    {STATE_BIND_EXECUTE, startBind, 0, bind_execute_events, ARRAY_SIZE(bind_execute_events)},
    {STATE_BIND_STATUS, displayBindStatus, 1, bind_status_events, ARRAY_SIZE(bind_status_events)},

};

static std::stack<int> state_index_stack;
static int current_state_index = 0;
static uint32_t current_state_entered = 0;

void startFSM(uint32_t now)
{
    state_index_stack.push(STATE_IGNORED);
    FSM[current_state_index].entry();
    current_state_entered = now;
}

void handleEvent(uint32_t now, fsm_event_t event)
{
    // Event timeout has not occurred
    if (event == EVENT_TIMEOUT && now - current_state_entered < FSM[current_state_index].timeout * 1000)
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

#if defined(RADIO_SX128X)
const char *rate_string[] = {
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

const char *message_string[] = {
    "ExpressLRS",
    "[  Connected  ]",
    "[  ! Armed !  ]",
    "[  Mismatch!  ]"
};

const char *main_menu_line_1[] = {
    "PACKET",
    "TX",
    "TELEM",
    "MOTION",
    "FAN",
    "BIND",
    "UPDATE"
};

const char *main_menu_line_2[] = {
    "RATE",
    "POWER",
    "RATIO",
    "DETECT",
    "CONTROL",
    "MODE",
    "FW"
};

static void displaySplashScreen()
{
}

static void displayIdleScreen()
{
}

static void displayMenuScreen(int menuItem)
{
}

// Value menu
static const char **values;
static int values_count;
static int values_index;

static void setupValueIndex()
{
    switch (state_index_stack.top())
    {
    case STATE_PACKET:
        values = rate_string;
        values_count = ARRAY_SIZE(rate_string);
        values_index = config.GetRate();
        break;
    case STATE_POWER:
        values = power_string;
        values_count = ARRAY_SIZE(power_string);
        values_index = config.GetPower();
        break;
    case STATE_TELEMETRY:
        values = ratio_string;
        values_count = ARRAY_SIZE(ratio_string);
        values_index = config.GetTlm();
        break;
    case STATE_POWERSAVE:
        values = powersaving_string;
        values_count = ARRAY_SIZE(powersaving_string);
        values_index = config.GetFanMode();
        break;
    case STATE_SMARTFAN:
        values = smartfan_string;
        values_count = ARRAY_SIZE(smartfan_string);
        values_index = config.GetMotionMode();
        break;
    }
}

static void displayValueIndex()
{
}

static void incrementValueIndex()
{
}

static void decrementValueIndex()
{
}

static void saveValueIndex()
{
}

// WiFi
static void startWiFi()
{
}

static void exitWiFi()
{
}

static void displayWiFiStatus()
{
}

// Bind
static void startBind()
{
}

static void displayBindStatus()
{
}
