#include "OLED/oleddisplay.h"
#include "TFT/tftdisplay.h"

#include "common.h"
#include "config.h"
#include "helpers.h"
#include "logging.h"
#include "POWERMGNT.h"

#ifdef HAS_THERMAL
#include "thermal.h"
#define UPDATE_TEMP_TIMEOUT 5000
extern Thermal thermal;
#endif

extern FiniteStateMachine state_machine;

extern bool ICACHE_RAM_ATTR IsArmed();
extern void EnterBindingMode();
extern bool InBindingMode;
extern bool RxWiFiReadyToSend;
extern bool TxBackpackWiFiReadyToSend;
extern bool VRxBackpackWiFiReadyToSend;
extern void VtxTriggerSend();

extern Display *display;

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
    display->displaySplashScreen();
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

        display->displayIdleScreen(changed, last_rate, last_power, last_tlm, last_motion, last_fan, last_dynamic, last_run_power, last_temperature, last_message);
    }
}

static void displayMenuScreen(bool init)
{
    display->displayMainMenu((menu_item_t)state_machine.getCurrentState());
}

// Value menu
static int values_min;
static int values_max;
static int values_index;

static void setupValueIndex(bool init)
{
    switch (state_machine.getParentState())
    {
    case STATE_PACKET:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetRate();
        break;
    case STATE_TELEMETRY:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetTlm();
        break;
    case STATE_POWERSAVE:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetMotionMode();
        break;
    case STATE_SMARTFAN:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetFanMode();
        break;

    case STATE_POWER_MAX:
        values_min = MinPower;
        values_max = MaxPower;
        values_index = config.GetPower();
        break;
    case STATE_POWER_DYNAMIC:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
        break;

    case STATE_VTX_BAND:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetVtxBand();
        break;
    case STATE_VTX_CHANNEL:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetVtxChannel();
        break;
    case STATE_VTX_POWER:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetVtxPower();
        break;
    case STATE_VTX_PITMODE:
        values_min = 0;
        values_max = display->getValueCount((menu_item_t)state_machine.getParentState())-1;
        values_index = config.GetVtxPitmode();
        break;
    }
}

static void displayValueIndex(bool init)
{
    display->displayValue((menu_item_t)state_machine.getParentState(), values_index);
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
    switch (state_machine.getParentState())
    {
        case STATE_PACKET:
            config.SetRate(values_index);
            break;
        case STATE_TELEMETRY:
            config.SetTlm(values_index);
            break;
        case STATE_POWERSAVE:
            config.SetMotionMode(values_index);
            break;
        case STATE_SMARTFAN:
            config.SetFanMode(values_index);
            break;

        case STATE_POWER_MAX:
            config.SetPower(values_index);
            break;
        case STATE_POWER_DYNAMIC:
            config.SetDynamicPower(values_index > 0);
            config.SetBoostChannel((values_index - 1) > 0 ? values_index - 1 : 0);
            break;

        case STATE_VTX_BAND:
            config.SetVtxBand(values_index);
            break;
        case STATE_VTX_CHANNEL:
            config.SetVtxChannel(values_index);
            break;
        case STATE_VTX_POWER:
            config.SetVtxPower(values_index);
            break;
        case STATE_VTX_PITMODE:
            config.SetVtxPitmode(values_index);
            break;
        default:
            break;
    }
}

// VTX Admin
static void executeSendVTX(bool init)
{
    if (init)
    {
        VtxTriggerSend();
        display->displayRunning();
    }
    else
    {
        state_machine.popState();
    }
}

// Bluetooth Joystck
static void displayBLEConfirm(bool init)
{
    display->displayBLEConfirm();
}

static void executeBLE(bool init)
{
    if (init)
    {
        connectionState = bleJoystick;
        display->displayBLEStatus();
    }
    else
    {
        if (connectionState != bleJoystick)
        {
            state_machine.popState();
        }
    }
}


static void exitBLE(bool init)
{
#ifdef PLATFORM_ESP32
    if (connectionState == bleJoystick) {
        rebootTime = millis() + 200;
    }
#endif
}

// WiFi
static void displayWiFiConfirm(bool init)
{
    display->displayWiFiConfirm();
}

static void exitWiFi(bool init)
{
#ifdef PLATFORM_ESP32
    if (connectionState == wifiUpdate) {
        rebootTime = millis() + 200;
    }
#endif
}

static void executeWiFi(bool init)
{
    bool running;
    if (init)
    {
        switch (state_machine.getParentState())
        {
            case STATE_WIFI_TX:
                connectionState = wifiUpdate;
                break;
            case STATE_WIFI_RX:
                RxWiFiReadyToSend = true;
                break;
            case STATE_WIFI_BACKPACK:
                TxBackpackWiFiReadyToSend = true;
                break;
            case STATE_WIFI_VRX:
                VRxBackpackWiFiReadyToSend = true;
                break;
        }
        if (state_machine.getParentState() == STATE_WIFI_TX)
        {
            display->displayWiFiStatus();
        }
        else
        {
            display->displayRunning();
        }
        return;
    }
    switch (state_machine.getParentState())
    {
        case STATE_WIFI_TX:
            running = connectionState == wifiUpdate;
            break;
        case STATE_WIFI_RX:
            running = RxWiFiReadyToSend;
            break;
        case STATE_WIFI_BACKPACK:
            running = TxBackpackWiFiReadyToSend;
            break;
        case STATE_WIFI_VRX:
            running = VRxBackpackWiFiReadyToSend;
            break;
        default:
            running = false;
    }
    if (!running)
    {
        state_machine.popState();
    }
}

// Bind
static void displayBindConfirm(bool init)
{
    display->displayBindConfirm();
}

static void executeBind(bool init)
{
    if (init)
    {
        EnterBindingMode();
        display->displayBindStatus();
        return;
    }
    if (!InBindingMode)
    {
        state_machine.popState();
    }
}


//-------------------------------------------------------------------

#define MENU_EVENTS(fsm) \
    {EVENT_TIMEOUT, ACTION_POPALL}, \
    {EVENT_LEFT, ACTION_POP}, \
    {EVENT_ENTER, PUSH(fsm)}, \
    {EVENT_RIGHT, PUSH(fsm)}, \
    {EVENT_UP, ACTION_PREVIOUS}, \
    {EVENT_DOWN, ACTION_NEXT}


// Value submenu FSM
fsm_state_event_t const value_init_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_select_events[] = {
    {EVENT_TIMEOUT, ACTION_POPALL},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_VALUE_SAVE)},
    {EVENT_UP, GOTO(STATE_VALUE_DEC)},
    {EVENT_DOWN, GOTO(STATE_VALUE_INC)}
};
fsm_state_event_t const value_increment_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_decrement_events[] = {{EVENT_IMMEDIATE, GOTO(STATE_VALUE_SELECT)}};
fsm_state_event_t const value_save_events[] = {{EVENT_IMMEDIATE, ACTION_POP}};

fsm_state_entry_t const value_select_fsm[] = {
    {STATE_VALUE_INIT, nullptr, setupValueIndex, 0, value_init_events, ARRAY_SIZE(value_init_events)},
    {STATE_VALUE_SELECT, nullptr, displayValueIndex, 20000, value_select_events, ARRAY_SIZE(value_select_events)},
    {STATE_VALUE_INC, nullptr, incrementValueIndex, 0, value_increment_events, ARRAY_SIZE(value_increment_events)},
    {STATE_VALUE_DEC, nullptr, decrementValueIndex, 0, value_decrement_events, ARRAY_SIZE(value_decrement_events)},
    {STATE_VALUE_SAVE, nullptr, saveValueIndex, 0, value_save_events, ARRAY_SIZE(value_save_events)},
    {STATE_LAST}
};

fsm_state_event_t const value_menu_events[] = {MENU_EVENTS(value_select_fsm)};

// Power FSM
fsm_state_entry_t const power_menu_fsm[] = {
    {STATE_POWER_MAX, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_POWER_DYNAMIC, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_LAST}
};

// VTX Admin FSM
fsm_state_event_t const vtx_execute_events[] = {{EVENT_TIMEOUT, GOTO(STATE_VTX_SEND)}, {EVENT_LEFT, ACTION_POP}};
fsm_state_entry_t const vtx_execute_fsm[] = {
    {STATE_VTX_SEND, nullptr, executeSendVTX, 1000, vtx_execute_events, ARRAY_SIZE(vtx_execute_events)},
};

fsm_state_event_t const vtx_send_events[] = {MENU_EVENTS(vtx_execute_fsm)};
fsm_state_entry_t const vtx_menu_fsm[] = {
    {STATE_VTX_BAND, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_VTX_CHANNEL, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_VTX_POWER, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_VTX_PITMODE, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_VTX_SEND, nullptr, displayMenuScreen, 20000, vtx_send_events, ARRAY_SIZE(vtx_send_events)},
    {STATE_LAST}
};

// BLE Joystick FSM
fsm_state_event_t const ble_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POPALL},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_BLE_EXECUTE)}
};
fsm_state_event_t const ble_execute_events[] = {{EVENT_TIMEOUT, GOTO(STATE_BLE_EXECUTE)}, {EVENT_LEFT, GOTO(STATE_BLE_EXIT)}};
fsm_state_event_t const ble_exit_events[] = {{EVENT_IMMEDIATE, ACTION_POP}};

fsm_state_entry_t const ble_menu_fsm[] = {
    {STATE_BLE_CONFIRM, nullptr, displayBLEConfirm, 20000, ble_confirm_events, ARRAY_SIZE(ble_confirm_events)},
    {STATE_BLE_EXECUTE, nullptr, executeBLE, 1000, ble_execute_events, ARRAY_SIZE(ble_execute_events)},
    {STATE_BLE_EXIT, nullptr, exitBLE, 0, ble_exit_events, ARRAY_SIZE(ble_exit_events)},
    {STATE_LAST}
};

// WiFi Update FSM
fsm_state_event_t const wifi_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POPALL},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_WIFI_EXECUTE)}
};
fsm_state_event_t const wifi_execute_events[] = {{EVENT_TIMEOUT, GOTO(STATE_WIFI_EXECUTE)}, {EVENT_LEFT, GOTO(STATE_WIFI_EXIT)}};
fsm_state_event_t const wifi_exit_events[] = {{EVENT_IMMEDIATE, ACTION_POP}};

fsm_state_entry_t const wifi_update_menu_fsm[] = {
    {STATE_WIFI_CONFIRM, nullptr, displayWiFiConfirm, 20000, wifi_confirm_events, ARRAY_SIZE(wifi_confirm_events)},
    {STATE_WIFI_EXECUTE, nullptr, executeWiFi, 1000, wifi_execute_events, ARRAY_SIZE(wifi_execute_events)},
    {STATE_WIFI_EXIT, nullptr, exitWiFi, 0, wifi_exit_events, ARRAY_SIZE(wifi_exit_events)},
    {STATE_LAST}
};
fsm_state_event_t const wifi_menu_update_events[] = {MENU_EVENTS(wifi_update_menu_fsm)};
fsm_state_event_t const wifi_ext_execute_events[] = {{EVENT_TIMEOUT, GOTO(STATE_WIFI_EXECUTE)}};
fsm_state_entry_t const wifi_ext_menu_fsm[] = {
    {STATE_WIFI_EXECUTE, nullptr, executeWiFi, 1000, wifi_ext_execute_events, ARRAY_SIZE(wifi_ext_execute_events)},
    {STATE_LAST}
};
fsm_state_event_t const wifi_ext_menu_events[] = {MENU_EVENTS(wifi_ext_menu_fsm)};
fsm_state_entry_t const wifi_menu_fsm[] = {
#if defined(PLATFORM_ESP32)
    {STATE_WIFI_TX, nullptr, displayMenuScreen, 20000, wifi_menu_update_events, ARRAY_SIZE(wifi_menu_update_events)},
#endif
    {STATE_WIFI_RX, nullptr, displayMenuScreen, 20000, wifi_ext_menu_events, ARRAY_SIZE(wifi_ext_menu_events)},
#if defined(USE_TX_BACKPACK)
    {STATE_WIFI_BACKPACK, [](){return OPT_USE_TX_BACKPACK;}, displayMenuScreen, 20000, wifi_ext_menu_events, ARRAY_SIZE(wifi_ext_menu_events)},
    {STATE_WIFI_VRX, [](){return OPT_USE_TX_BACKPACK;}, displayMenuScreen, 20000, wifi_ext_menu_events, ARRAY_SIZE(wifi_ext_menu_events)},
#endif
    {STATE_LAST}
};

// Bind FSM
fsm_state_event_t const bind_confirm_events[] = {
    {EVENT_TIMEOUT, ACTION_POPALL},
    {EVENT_LEFT, ACTION_POP},
    {EVENT_ENTER, GOTO(STATE_BIND_EXECUTE)}
};
fsm_state_event_t const bind_execute_events[] = {{EVENT_TIMEOUT, GOTO(STATE_BIND_EXECUTE)}};

fsm_state_entry_t const bind_menu_fsm[] = {
    {STATE_BIND_CONFIRM, nullptr, displayBindConfirm, 20000, bind_confirm_events, ARRAY_SIZE(bind_confirm_events)},
    {STATE_BIND_EXECUTE, nullptr, executeBind, 1000, bind_execute_events, ARRAY_SIZE(bind_execute_events)},
    {STATE_LAST}
};

// Main menu FSM
fsm_state_event_t const power_menu_events[] = {MENU_EVENTS(power_menu_fsm)};
fsm_state_event_t const vtx_menu_events[] = {MENU_EVENTS(vtx_menu_fsm)};
fsm_state_event_t const ble_menu_events[] = {MENU_EVENTS(ble_menu_fsm)};
fsm_state_event_t const bind_menu_events[] = {MENU_EVENTS(bind_menu_fsm)};
fsm_state_event_t const wifi_menu_events[] = {MENU_EVENTS(wifi_menu_fsm)};

fsm_state_entry_t const main_menu_fsm[] = {
    {STATE_PACKET, nullptr, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
    {STATE_POWER, nullptr, displayMenuScreen, 20000, power_menu_events, ARRAY_SIZE(power_menu_events)},
#if !defined(USE_AIRPORT_AT_BAUD)
    {STATE_TELEMETRY, nullptr,displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#ifdef HAS_GSENSOR
    {STATE_POWERSAVE, [](){return OPT_HAS_GSENSOR;}, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#endif
#endif
#ifdef HAS_THERMAL
    {STATE_SMARTFAN, [](){return OPT_HAS_THERMAL;}, displayMenuScreen, 20000, value_menu_events, ARRAY_SIZE(value_menu_events)},
#endif
#if !defined(USE_AIRPORT_AT_BAUD)
    {STATE_VTX, nullptr, displayMenuScreen, 20000, vtx_menu_events, ARRAY_SIZE(vtx_menu_events)},
    {STATE_JOYSTICK, nullptr, displayMenuScreen, 20000, ble_menu_events, ARRAY_SIZE(ble_menu_events)},
#endif
    {STATE_BIND, nullptr, displayMenuScreen, 20000, bind_menu_events, ARRAY_SIZE(bind_menu_events)},
    {STATE_WIFI, nullptr, displayMenuScreen, 20000, wifi_menu_events, ARRAY_SIZE(wifi_menu_events)},
    {STATE_LAST}
};

// Entry FSM
fsm_state_event_t const splash_events[] = {
    {EVENT_TIMEOUT, GOTO(STATE_IDLE)}
};
fsm_state_event_t const idle_events[] = {
    {EVENT_TIMEOUT, GOTO(STATE_IDLE)},
    {EVENT_LONG_ENTER, PUSH(main_menu_fsm)},
    {EVENT_LONG_RIGHT, PUSH(main_menu_fsm)}
};
fsm_state_entry_t const entry_fsm[] = {
    {STATE_SPLASH, nullptr, displaySplashScreen, 5000, splash_events, ARRAY_SIZE(splash_events)},
    {STATE_IDLE, nullptr, displayIdleScreen, 100, idle_events, ARRAY_SIZE(idle_events)},
    {STATE_LAST}
};
