
#include "fsm.h"

// -------------- States --------------
#define STATE_SPLASH 0
#define STATE_IDLE 1
#define STATE_PACKET 2
#define STATE_POWER 3
#define STATE_TELEMETRY 4
#define STATE_POWERSAVE 5
#define STATE_SMARTFAN 6
#define STATE_BIND 7
#define STATE_WIFI 8

#define STATE_WIFI_CONFIRM 20
#define STATE_WIFI_EXECUTE 21
#define STATE_WIFI_STATUS 22
#define STATE_WIFI_EXIT 23

#define STATE_BIND_CONFIRM 30
#define STATE_BIND_EXECUTE 31
#define STATE_BIND_STATUS 32

#define STATE_VALUE_INIT 100
#define STATE_VALUE_SELECT 101
#define STATE_VALUE_INC 102
#define STATE_VALUE_DEC 103
#define STATE_VALUE_SAVE 104

// -------------- Events --------------
#define LONG_PRESSED 0x40

#define EVENT_ENTER 0
#define EVENT_UP 1
#define EVENT_DOWN 2
#define EVENT_LEFT 3
#define EVENT_RIGHT 4
#define EVENT_LONG_ENTER (EVENT_ENTER | LONG_PRESSED)
#define EVENT_LONG_UP (EVENT_UP | LONG_PRESSED)
#define EVENT_LONG_DOWN (EVENT_DOWN | LONG_PRESSED)
#define EVENT_LONG_LEFT (EVENT_LEFT | LONG_PRESSED)
#define EVENT_LONG_RIGHT (EVENT_RIGHT | LONG_PRESSED)

extern fsm_state_entry_t const menu_fsm[];

fsm_state_t getInitialState();