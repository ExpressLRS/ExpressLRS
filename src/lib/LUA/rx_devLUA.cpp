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

//----------------------------Info-----------------------------------

static struct luaItem_string luaInfo = {
    {"Bad/Good", (crsf_value_type_e)(CRSF_INFO | CRSF_FIELD_ELRS_HIDDEN)},
    emptySpace
};

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


extern RxConfig config;
extern bool RxWiFiReadyToSend;
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern unsigned long rebootTime;
extern void beginWebsever();
#endif

static void registerLuaParameters()
{
  registerLUAParameter(&luaAirRate, [](uint8_t id, uint8_t arg){
  });
  registerLUAParameter(&luaRxWebUpdate, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        connectionState = wifiUpdate;
#endif
    }
    sendLuaCommandResponse(&luaRxWebUpdate, arg < 5 ? 2 : 0, arg < 5 ? "Sending..." : "");
  });
  registerLUAParameter(&luaInfo);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event()
{
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
