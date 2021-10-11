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

static struct luaItem_Selection luaAirRate = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Packet Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "25(-123dbm);50(-120dbm);100(-117dbm);200(-112dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "50(-117dbm);150(-112dbm);250(-108dbm);500(-105dbm)",
#endif
    "Hz"
};

static struct luaItem_Selection luaTlmRate = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Telem Ratio",
    "Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2",
    emptySpace
};

//----------------------------POWER------------------
static struct tagLuaItem_folder luaPowerFolder = {
    {CRSF_FOLDER},
    "TX Power",
    LUA_FOLDER_SIZE(luaPowerFolder)
};

static struct luaItem_Selection luaPower = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Max Power",
    "10;25;50;100;250;500;1000;2000",
    "mW"
};

static struct luaItem_Selection luaDynamicPower = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Dynamic",
    "Off;On;AUX9;AUX10;AUX11;AUX12",
    emptySpace
};
//----------------------------POWER------------------

static struct luaItem_Selection luaSwitch = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Switch Mode",
    "Hybrid;Wide",
    emptySpace
};

static struct luaItem_Selection luaModelMatch = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Model Match",
    "Off;On",
    emptySpace
};
static struct tagLuaItem_command luaBind = {
    {CRSF_COMMAND},
    "Bind",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaBind)
};

static struct tagLuaItem_string luaInfo = {
    {(crsf_value_type_e)(CRSF_INFO | CRSF_FIELD_ELRS_HIDDEN)},
    "Bad/Good",
    thisCommit,
    LUA_STRING_SIZE(luaInfo)
};

static struct tagLuaItem_string luaELRSversion = {
    {CRSF_INFO},
    thisVersion,
    thisCommit,
    LUA_STRING_SIZE(luaELRSversion)
};

//---------------------------- WiFi -----------------------------
static struct tagLuaItem_folder luaWiFiFolder = {
    {CRSF_FOLDER},
    "WiFi",
    LUA_FOLDER_SIZE(luaWiFiFolder)
};

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
static struct tagLuaItem_command luaWebUpdate = {
    {CRSF_COMMAND},
    "WiFi Tx",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaWebUpdate)
};
#endif

static struct tagLuaItem_command luaTxBackpackUpdate = {
    {CRSF_COMMAND},
    "WiFi Tx Backpack",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaTxBackpackUpdate)
};

static struct tagLuaItem_command luaVRxBackpackUpdate = {
    {CRSF_COMMAND},
    "WiFi VRx Backpack",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaVRxBackpackUpdate)
};
//---------------------------- WiFi -----------------------------

#if defined(PLATFORM_ESP32)
static struct tagLuaItem_command luaBLEJoystick = {
    {CRSF_COMMAND},
    "BLE Joystick",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaBLEJoystick)
};
#endif

//----------------------------VTX ADMINISTRATOR------------------
static struct tagLuaItem_folder luaVtxFolder = {
    {CRSF_FOLDER},
    "VTX Administrator",
    LUA_FOLDER_SIZE(luaVtxFolder)
};

static struct luaItem_Selection luaVtxBand = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Band",
    "Off;A;B;E;F;R;L",
    emptySpace
};

static struct luaItem_Selection luaVtxChannel = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Channel",
    "1;2;3;4;5;6;7;8",
    emptySpace
};

static struct luaItem_Selection luaVtxPwr = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Pwr Lvl",
    "-;1;2;3;4;5;6;7;8",
    emptySpace
};

static struct luaItem_Selection luaVtxPit = {
    {CRSF_TEXT_SELECTION},
    0, // value
    "Pitmode",
    "Off;On",
    emptySpace
};

static struct tagLuaItem_command luaVtxSend = {
    {CRSF_COMMAND},
    "Send VTx",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaVtxSend)
};
//----------------------------VTX ADMINISTRATOR------------------

static char luaBadGoodString[10];

extern TxConfig config;
extern uint8_t VtxConfigReadyToSend;
extern uint8_t TxBackpackWiFiReadyToSend;
extern uint8_t VRxBackpackWiFiReadyToSend;
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void SetSyncSpam();
extern void EnterBindingMode();
extern bool InBindingMode;
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
extern void beginWebsever();
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
  }, luaPowerFolder.common.id);
  registerLUAParameter(&luaDynamicPower, [](uint8_t id, uint8_t arg){
      config.SetDynamicPower(arg > 0);
      config.SetBoostChannel((arg - 1) > 0 ? arg - 1 : 0);
  }, luaPowerFolder.common.id);
  registerLUAParameter(&luaVtxFolder);
  registerLUAParameter(&luaVtxBand, [](uint8_t id, uint8_t arg){
      config.SetVtxBand(arg);
  },luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxChannel, [](uint8_t id, uint8_t arg){
      config.SetVtxChannel(arg);
  },luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxPwr, [](uint8_t id, uint8_t arg){
      config.SetVtxPower(arg);
  },luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxPit, [](uint8_t id, uint8_t arg){
      config.SetVtxPitmode(arg);
  },luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxSend, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      VtxConfigReadyToSend = true;
    }
    sendLuaCommandResponse(&luaVtxSend, arg < 5 ? 2 : 0, arg < 5 ? "Sending..." : "");
  },luaVtxFolder.common.id);

  registerLUAParameter(&luaBind, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      DBGLN("Binding requested from LUA");
      EnterBindingMode();
    }
    sendLuaCommandResponse(&luaBind, arg < 5 ? 2 : 0, arg < 5 ? "Binding..." : "");
  });

  registerLUAParameter(&luaWiFiFolder);
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
    },luaWiFiFolder.common.id);
  #endif
  registerLUAParameter(&luaTxBackpackUpdate, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      TxBackpackWiFiReadyToSend = true;
    }
    sendLuaCommandResponse(&luaTxBackpackUpdate, arg < 5 ? 2 : 0, arg < 5 ? "Sending..." : "");
  },luaWiFiFolder.common.id);

  registerLUAParameter(&luaVRxBackpackUpdate, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      VRxBackpackWiFiReadyToSend = true;
    }
    sendLuaCommandResponse(&luaVRxBackpackUpdate, arg < 5 ? 2 : 0, arg < 5 ? "Sending..." : "");
  },luaWiFiFolder.common.id);

  #ifdef PLATFORM_ESP32
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

static int event()
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

static int timeout()
{
  if(luaHandleUpdateParameter())
  {
    SetSyncSpam();
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
