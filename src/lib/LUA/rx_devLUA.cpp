#ifdef TARGET_RX

#include "common.h"
#include "device.h"

#include "CRSF.h"
#include "POWERMGNT.h"
#include "config.h"
#include "logging.h"
#include "lua.h"
#include "OTA.h"
#include "hwTimer.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#else
#error "Radio configuration is not valid!"
#endif

static const char thisCommit[] = {LATEST_COMMIT, 0};
static const char thisVersion[] = {LATEST_VERSION, 0};
static const char emptySpace[1] = {0};
static char strPowerLevels[] = "10;25;50;100;250;500;1000;2000";

static struct luaItem_selection luaAirRate = {
    {"Packet Rate", CRSF_TEXT_SELECTION},
    0, // value
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    "25(-123dbm);50(-120dbm);100(-117dbm);200(-112dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "50(-117dbm);150(-112dbm);250(-108dbm);500(-105dbm)",
#endif
    "Hz"
};

static struct luaItem_selection luaTlmRate = {
    {"Telem Ratio", CRSF_TEXT_SELECTION},
    0, // value
    "Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2",
    emptySpace
};

//----------------------------POWER------------------
static struct luaItem_folder luaPowerFolder = {
    {"TX Power", CRSF_FOLDER},
};

static struct luaItem_selection luaPower = {
    {"Max Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};

static struct luaItem_selection luaDynamicPower = {
    {"Dynamic", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On;AUX9;AUX10;AUX11;AUX12",
    emptySpace
};

#if defined(GPIO_PIN_FAN_EN)
static struct luaItem_selection luaFanThreshold = {
    {"Fan Thresh", CRSF_TEXT_SELECTION},
    0, // value
    "10mW;25mW;50mW;100mW;250mW;500mW;1000mW;2000mW;Never",
    emptySpace // units embedded so it won't display "NevermW"
};
#endif

//----------------------------POWER------------------

static struct luaItem_selection luaSwitch = {
    {"Switch Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Hybrid;Wide",
    emptySpace
};

static struct luaItem_selection luaModelMatch = {
    {"Model Match", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    emptySpace
};

static struct luaItem_command luaBind = {
    {"Bind", CRSF_COMMAND},
    0, // step
    emptySpace
};

static struct luaItem_string luaInfo = {
    {"Bad/Good", (crsf_value_type_e)(CRSF_INFO | CRSF_FIELD_ELRS_HIDDEN)},
    emptySpace
};

static struct luaItem_string luaELRSversion = {
    {thisVersion, CRSF_INFO},
    thisCommit
};

//---------------------------- WiFi -----------------------------
static struct luaItem_folder luaWiFiFolder = {
    {"WiFi Connectivity", CRSF_FOLDER}
};

#if defined(PLATFORM_ESP32)
static struct luaItem_command luaWebUpdate = {
    {"Enable WiFi", CRSF_COMMAND},
    0, // step
    emptySpace
};
#endif

static struct luaItem_command luaRxWebUpdate = {
    {"Enable Rx WiFi", CRSF_COMMAND},
    0, // step
    emptySpace
};

#if defined(USE_TX_BACKPACK)
static struct luaItem_command luaTxBackpackUpdate = {
    {"Enable Backpack WiFi", CRSF_COMMAND},
    0, // step
    emptySpace
};

static struct luaItem_command luaVRxBackpackUpdate = {
    {"Enable VRx WiFi", CRSF_COMMAND},
    0, // step
    emptySpace
};
#endif // USE_TX_BACKPACK
//---------------------------- WiFi -----------------------------

#if defined(PLATFORM_ESP32)
static struct luaItem_command luaBLEJoystick = {
    {"BLE Joystick", CRSF_COMMAND},
    0, // step
    emptySpace
};
#endif

//----------------------------VTX ADMINISTRATOR------------------
static struct luaItem_folder luaVtxFolder = {
    {"VTX Administrator", CRSF_FOLDER},
};

static struct luaItem_selection luaVtxBand = {
    {"Band", CRSF_TEXT_SELECTION},
    0, // value
    "Off;A;B;E;F;R;L",
    emptySpace
};

static struct luaItem_selection luaVtxChannel = {
    {"Channel", CRSF_TEXT_SELECTION},
    0, // value
    "1;2;3;4;5;6;7;8",
    emptySpace
};

static struct luaItem_selection luaVtxPwr = {
    {"Pwr Lvl", CRSF_TEXT_SELECTION},
    0, // value
    "-;1;2;3;4;5;6;7;8",
    emptySpace
};

static struct luaItem_selection luaVtxPit = {
    {"Pitmode", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    emptySpace
};

static struct luaItem_command luaVtxSend = {
    {"Send VTx", CRSF_COMMAND},
    0, // step
    emptySpace
};
//----------------------------VTX ADMINISTRATOR------------------

#if defined(TARGET_TX_FM30)
struct luaItem_selection luaBluetoothTelem = {
    {"BT Telemetry", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    emptySpace
};
#endif


static char luaBadGoodString[10];

extern RxConfig config;
extern void VtxTriggerSend();
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void EnterBindingMode();
extern bool InBindingMode;
extern bool connectionHasModelMatch;
extern bool RxWiFiReadyToSend;
#if defined(USE_TX_BACKPACK)
extern bool TxBackpackWiFiReadyToSend;
extern bool VRxBackpackWiFiReadyToSend;
#endif
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
extern void beginWebsever();
#endif

static void registerLuaParameters()
{
  registerLUAParameter(&luaAirRate, [](uint8_t id, uint8_t arg){
  });
  #if defined(TARGET_TX_FM30)
  registerLUAParameter(&luaBluetoothTelem, [](uint8_t id, uint8_t arg) {
    digitalWrite(GPIO_PIN_BLUETOOTH_EN, !arg);
    devicesTriggerEvent();
  });
  #endif
  // POWER folder
#if defined(GPIO_PIN_FAN_EN)
  registerLUAParameter(&luaFanThreshold, [](uint8_t id, uint8_t arg){
      config.SetPowerFanThreshold(arg);
      POWERMGNT::setFanEnableTheshold((PowerLevels_e)arg);
  }, luaPowerFolder.common.id);
#endif
  registerLUAParameter(NULL);
}

static int event()
{
  setLuaWarningFlag(LUA_FLAG_MODEL_MATCH, connectionState == connected && connectionHasModelMatch == false);
  setLuaWarningFlag(LUA_FLAG_CONNECTED, connectionState == connected);
#if defined(GPIO_PIN_FAN_EN)
  setLuaTextSelectionValue(&luaFanThreshold, config.GetPowerFanThreshold());
#endif

  #if defined(TARGET_TX_FM30)
    setLuaTextSelectionValue(&luaBluetoothTelem, !digitalRead(GPIO_PIN_BLUETOOTH_EN));
  #endif
  return DURATION_IMMEDIATELY;
}

static int timeout()
{
  luaHandleUpdateParameter();
  
  return DURATION_IMMEDIATELY;
}

static int start()
{
  registerLuaParameters();
  event();
  return DURATION_IMMEDIATELY;
}

device_t LUA_device = {
  .initialize = NULL,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif
