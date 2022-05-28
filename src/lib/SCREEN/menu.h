
#include "fsm.h"

// -------------- States --------------

enum fsm_state_s {
    // This first ones have menu text & icons (and are used as array indexes)
    STATE_PACKET = 0,
    STATE_POWER,
    STATE_TELEMETRY,
    STATE_POWERSAVE,
    STATE_SMARTFAN,
    STATE_JOYSTICK,
    STATE_BIND,
    STATE_WIFI,
    STATE_VTX,

    STATE_POWER_MAX,
    STATE_POWER_DYNAMIC,

    STATE_VTX_BAND,
    STATE_VTX_CHANNEL,
    STATE_VTX_POWER,
    STATE_VTX_PITMODE,
    STATE_VTX_SEND,

    STATE_WIFI_TX,
    STATE_WIFI_RX,
    STATE_WIFI_BACKPACK,
    STATE_WIFI_VRX,


    // These do not have menu text or icons
    STATE_SPLASH = 100,
    STATE_IDLE,

    STATE_BLE_CONFIRM,
    STATE_BLE_EXECUTE,
    STATE_BLE_EXIT,

    STATE_WIFI_CONFIRM,
    STATE_WIFI_EXECUTE,
    STATE_WIFI_EXIT,

    STATE_BIND_CONFIRM,
    STATE_BIND_EXECUTE,

    STATE_VTX_SAVE,

    STATE_VALUE_INIT,
    STATE_VALUE_SELECT,
    STATE_VALUE_INC,
    STATE_VALUE_DEC,
    STATE_VALUE_SAVE,
};


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

extern fsm_state_entry_t const entry_fsm[];

fsm_state_t getInitialState();