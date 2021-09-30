#ifdef TARGET_TX

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

struct tagLuaItem_textSelection luaAirRate = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION}, //id,type
    "Packet Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "25(-123dbm);50(-120dbm);100(-117dbm);200(-112dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "50(-117dbm);150(-112dbm);250(-108dbm);500(-105dbm)",
#endif
    {0,0,3},//value,min,max
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};
struct tagLuaItem_textSelection luaTlmRate = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Telem Ratio",
    "Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2",
    {0,0,7},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
//----------------------------POWER------------------
struct tagLuaItem_folder luaPowerFolder = {
    {0,0,(uint8_t)CRSF_FOLDER},//id,type
    "TX Power",
    LUA_FOLDER_SIZE(luaPowerFolder)
};
struct tagLuaItem_textSelection luaPower = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Max Power",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7},//value,min,max
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};
struct tagLuaItem_textSelection luaDynamicPower = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Dynamic",
    "Off;On;AUX9;AUX10;AUX11;AUX12",
    {0,0,5},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaDynamicPower)
};
//----------------------------POWER------------------

struct tagLuaItem_textSelection luaSwitch = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Switch Mode",
    "Hybrid;Wide",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaSwitch)
};
struct tagLuaItem_textSelection luaModelMatch = {
    {5,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Model Match",
    "Off;On",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaModelMatch)
};
struct tagLuaItem_command luaBind = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "Bind",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaBind)
};
struct tagLuaItem_string luaInfo = {
    {0,0,(uint8_t)CRSF_INFO|CRSF_FIELD_ELRS_HIDDEN},//id,type
    "Bad/Good",
    thisCommit,
    LUA_STRING_SIZE(luaInfo)
};
struct tagLuaItem_string luaELRSversion = {
    {0,0,(uint8_t)CRSF_INFO},//id,type
    thisVersion,
    thisCommit,
    LUA_STRING_SIZE(luaELRSversion)
};

#ifdef PLATFORM_ESP32
struct tagLuaItem_command luaWebUpdate = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "WiFi Update",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaWebUpdate)
};

struct tagLuaItem_command luaBLEJoystick = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "BLE Joystick",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaBLEJoystick)
};
#endif

//----------------------------VTX ADMINISTRATOR------------------
struct tagLuaItem_folder luaVtxFolder = {
    {0,0,(uint8_t)CRSF_FOLDER},//id,type
    "VTX Administrator",
    LUA_FOLDER_SIZE(luaVtxFolder)
};

struct tagLuaItem_textSelection luaVtxBand = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Band",
    "Off;A;B;E;F;R;L",
    {0,0,6},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxBand)
};

struct tagLuaItem_textSelection luaVtxChannel = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Channel",
    "1;2;3;4;5;6;7;8",
    {0,0,7},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxChannel)
};

struct tagLuaItem_textSelection luaVtxPwr = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Pwr Lvl",
    "-;1;2;3;4;5;6;7;8",
    {0,0,8},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxPwr)
};

struct tagLuaItem_textSelection luaVtxPit = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Pitmode",
    "Off;On",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxPit)
};

struct tagLuaItem_command luaVtxSend = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "Send VTx",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaVtxSend)
};

//----------------------------VTX ADMINISTRATOR------------------

static char luaBadGoodString[10];
extern TxConfig config;
extern uint8_t adjustPacketRateForBaud(uint8_t rate);

extern TxConfig config;
extern uint8_t VtxConfigReadyToSend;
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void EnterBindingMode();
extern bool InBindingMode;
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
extern void beginWebsever();
extern bool eventFired;
#endif

static void registerLuaParameters() {
  registerLUAParameter(&luaAirRate, [](uint8_t id, uint8_t arg){
    if ((arg < RATE_MAX) && (arg >= 0))
    {
      DBGLN("Request AirRate: %d", arg);
      uint8_t rate = RATE_MAX - 1 - arg;
      rate = adjustPacketRateForBaud(rate);
      config.SetRate(rate);
    }
  });
  registerLUAParameter(&luaTlmRate, [](uint8_t id, uint8_t arg){
    if ((arg <= (uint8_t)TLM_RATIO_1_2) && (arg >= (uint8_t)TLM_RATIO_NO_TLM))
    {
      DBGLN("Request TLM interval: %d", arg);
      config.SetTlm((expresslrs_tlm_ratio_e)arg);
    }
  });
  registerLUAParameter(&luaSwitch, [](uint8_t id, uint8_t arg){
    // Only allow changing switch mode when disconnected since we need to guarantee
    // the pack and unpack functions are matched
    if (connectionState == disconnected)
    {
      DBGLN("Request Switch Mode: %d", arg);
      // +1 to the mode because 1-bit was mode 0 and has been removed
      // The modes should be updated for 1.1RC so mode 0 can be smHybrid
      uint32_t newSwitchMode = (CRSF::ParameterUpdateData[2] + 1) & 0b11;
      config.SetSwitchMode(newSwitchMode);
      OtaSetSwitchMode((OtaSwitchMode_e)newSwitchMode);
    }
  });
  registerLUAParameter(&luaModelMatch, [](uint8_t id, uint8_t arg){
    bool newModelMatch = CRSF::ParameterUpdateData[2] & 0b1;
#ifndef DEBUG_SUPPRESS
    DBGLN("Request Model Match: %d", newModelMatch);
#endif
    config.SetModelMatch(newModelMatch);
    if (connectionState == connected) {
      mspPacket_t msp;
      msp.reset();
      msp.makeCommand();
      msp.function = MSP_SET_RX_CONFIG;
      msp.addByte(MSP_ELRS_MODEL_ID);
      msp.addByte(newModelMatch ? CRSF::getModelID() : 0xff);
      CRSF::AddMspMessage(&msp);
    }
  });
  registerLUAParameter(&luaPowerFolder);
  registerLUAParameter(&luaPower, [](uint8_t id, uint8_t arg){
    PowerLevels_e newPower = (PowerLevels_e)arg;
    DBGLN("Request Power: %d", newPower);
    
    if (newPower > MaxPower)
    {
        newPower = MaxPower;
    } else if (newPower < MinPower)
    {
        newPower = MinPower;
    }
    config.SetPower(newPower);
  }, luaPowerFolder.luaProperties1.id);
  registerLUAParameter(&luaDynamicPower, [](uint8_t id, uint8_t arg){
      config.SetDynamicPower(arg > 0);
      config.SetBoostChannel((arg - 1) > 0 ? arg - 1 : 0);
  }, luaPowerFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxFolder);
  registerLUAParameter(&luaVtxBand, [](uint8_t id, uint8_t arg){
      config.SetVtxBand(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxChannel, [](uint8_t id, uint8_t arg){
      config.SetVtxChannel(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxPwr, [](uint8_t id, uint8_t arg){
      config.SetVtxPower(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxPit, [](uint8_t id, uint8_t arg){
      config.SetVtxPitmode(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxSend, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      VtxConfigReadyToSend = true;
      sendLuaCommandResponse(&luaVtxSend, 2, "Sending...");
    } else {
      sendLuaCommandResponse(&luaVtxSend, 0, "Config Sent");
    }
  },luaVtxFolder.luaProperties1.id);

  registerLUAParameter(&luaBind, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      DBGLN("Binding requested from LUA");
      EnterBindingMode();
      sendLuaCommandResponse(&luaBind, 2, "Binding...");
    } else {
      sendLuaCommandResponse(&luaBind, InBindingMode ? 2 : 0, InBindingMode ? "Binding..." : "Binding Sent");
    }
  });
  #ifdef PLATFORM_ESP32
    registerLUAParameter(&luaWebUpdate, [](uint8_t id, uint8_t arg){
      DBGVLN("arg %d", arg);
      if (arg == 4) // 4 = request confirmed, start
      {
        //confirm run on ELRSv2.lua or start command from CRSF configurator,
        //since ELRS LUA can do 2 step confirmation, it needs confirmation to start wifi to prevent stuck on
        //unintentional button press.
        DBGLN("Wifi Update Mode Requested!");
        sendLuaCommandResponse(&luaWebUpdate, 2, "Wifi Running...");
        connectionState = wifiUpdate;
      }
      else if (arg > 0 && arg < 4)
      {
        sendLuaCommandResponse(&luaWebUpdate, 3, "Enter WiFi Update Mode?");
      }
      else if (arg == 5)
      {
        sendLuaCommandResponse(&luaWebUpdate, 0, "WiFi Cancelled");
        if (connectionState == wifiUpdate) {
          rebootTime = millis() + 400;
        }
      }
      else
      {
        sendLuaCommandResponse(&luaWebUpdate, luaWebUpdate.luaProperties2.status, luaWebUpdate.label2);
      }
    });

    registerLUAParameter(&luaBLEJoystick, [](uint8_t id, uint8_t arg){
      if (arg == 4) // 4 = request confirmed, start
      {
        //confirm run on ELRSv2.lua or start command from CRSF configurator,
        //since ELRS LUA can do 2 step confirmation, it needs confirmation to start BLE to prevent stuck on
        //unintentional button press.
        DBGLN("BLE Joystick Mode Requested!");
        sendLuaCommandResponse(&luaBLEJoystick, 2, "Joystick Running...");
        connectionState = bleJoystick;
      }
      else if (arg > 0 && arg < 4) //start command, 1 = start, 2 = running, 3 = request confirmation
      {
        sendLuaCommandResponse(&luaBLEJoystick, 3, "Start BLE Joystick?");
      }
      else if (arg == 5)
      {
        sendLuaCommandResponse(&luaBLEJoystick, 0, "Joystick Cancelled");
        if (connectionState == bleJoystick) {
          rebootTime = millis() + 400;
        }
      }
      else
      {
        sendLuaCommandResponse(&luaBLEJoystick, luaBLEJoystick.luaProperties2.status, luaBLEJoystick.label2);
      }
    });

  #endif

  registerLUAParameter(&luaInfo);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event(std::function<void ()> sendSpam)
{
  uint8_t rate = adjustPacketRateForBaud(config.GetRate());
  setLuaTextSelectionValue(&luaAirRate, RATE_MAX - 1 - rate);
  setLuaTextSelectionValue(&luaTlmRate, config.GetTlm());
  setLuaTextSelectionValue(&luaSwitch,(uint8_t)(config.GetSwitchMode() - 1)); // -1 for missing sm1Bit
  setLuaTextSelectionValue(&luaModelMatch,(uint8_t)config.GetModelMatch());

  setLuaTextSelectionValue(&luaPower, config.GetPower());

  uint8_t dynamic = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  setLuaTextSelectionValue(&luaDynamicPower,dynamic);

  setLuaTextSelectionValue(&luaVtxBand,config.GetVtxBand());
  setLuaTextSelectionValue(&luaVtxChannel,config.GetVtxChannel());
  setLuaTextSelectionValue(&luaVtxPwr,config.GetVtxPower());
  setLuaTextSelectionValue(&luaVtxPit,config.GetVtxPitmode());
  return DURATION_IMMEDIATELY;
}

static int timeout(std::function<void ()> sendSpam)
{
  if(luaHandleUpdateParameter())
  {
    sendSpam();
  }
  return DURATION_IMMEDIATELY;
}

static int start()
{
  CRSF::RecvParameterUpdate = &luaParamUpdateReq;
  registerLuaParameters();
  registerLUAPopulateParams([](){
    itoa(CRSF::BadPktsCountResult, luaBadGoodString, 10);
    strcat(luaBadGoodString, "/");
    itoa(CRSF::GoodPktsCountResult, luaBadGoodString + strlen(luaBadGoodString), 10);
    setLuaStringValue(&luaInfo, luaBadGoodString);
  });
  return DURATION_IMMEDIATELY;
}

device_t LUA_device = {
  .initialize = NULL,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif
