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

static const char thisCommit[] = {LATEST_COMMIT, 0};
static const char thisVersion[] = {LATEST_VERSION, 0};
static const char emptySpace[1] = {0};


static struct luaItem_selection luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    "Standard;Max Power",
    emptySpace
};

#if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
static struct luaItem_selection luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna B;Antenna A;Diversity",
    emptySpace
};
#endif

//----------------------------Info-----------------------------------

static struct luaItem_string luaELRSversion = {
    {thisVersion, CRSF_INFO},
    thisCommit
};

//----------------------------Info-----------------------------------

//---------------------------- WiFi -----------------------------

static struct luaItem_command luaRxWebUpdate = {
    {"Enable Rx WiFi", CRSF_COMMAND},
    0, // step
    emptySpace
};

//---------------------------- WiFi -----------------------------


extern POWERMGNT POWERMGNT;
extern RxConfig config;
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern unsigned long rebootTime;
extern void beginWebsever();
#endif

static void registerLuaParameters()
{

#if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
  registerLUAParameter(&luaAntennaMode, [](uint8_t id, uint8_t arg){
      config.SetAntennaMode(arg);
      config.Commit();// this commit doesn't trigger restart
      devicesTriggerEvent();
  });
#endif

registerLUAParameter(&luaTlmPower, [](uint8_t id, uint8_t arg){
    config.SetPower(arg);
    //config.Commit(); this commit trigger restart
    if(arg == 0){
      POWERMGNT.setPower(MinPower);
    } else {
      POWERMGNT.setPower(MaxPower);
    }
    devicesTriggerEvent();
  });

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
  registerLUAParameter(&luaRxWebUpdate, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
        connectionState = wifiUpdate;
    }
    sendLuaCommandResponse(&luaRxWebUpdate, arg < 5 ? 2 : 0, arg < 5 ? "Sending..." : "");
  });
#endif
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event()
{

#if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
  setLuaTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
#endif
  setLuaTextSelectionValue(&luaTlmPower, config.GetPower());
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
