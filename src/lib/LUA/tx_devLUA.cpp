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


static const char emptySpace[1] = {0};
static char strPowerLevels[] = "10;25;50;100;250;500;1000;2000";
char pwrFolderDynamicName[] = "TX Power (1000 Dynamic)";
char vtxFolderDynamicName[] = "VTX Admin (OFF:C:1 Aux11 )";
static const char folderNameSeparator[2] = {' ',':'};

static struct luaItem_selection luaAirRate = {
    {"Packet Rate", CRSF_TEXT_SELECTION},
    0, // value
#if defined(RADIO_SX127X)
    "25Hz(-123;50Hz(-120;100Hz(-117;200Hz(-112",
#elif defined(RADIO_SX128X)
    "50Hz(-117;150Hz(-112;250Hz(-108;500Hz(-105;F500(-104;F1000(-104",
#else
    #error Invalid radio configuration!
#endif
    "dBm)"
};

static struct luaItem_selection luaTlmRate = {
    {"Telem Ratio", CRSF_TEXT_SELECTION},
    0, // value
    "Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2",
    emptySpace
};

//----------------------------POWER------------------
static struct luaItem_folder luaPowerFolder = {
    {"TX Power", CRSF_FOLDER},pwrFolderDynamicName
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
    "Off;Dyn;AUX9;AUX10;AUX11;AUX12",
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

#if defined(Regulatory_Domain_EU_CE_2400)
static struct luaItem_string luaCELimit = {
    {"100mW CE LIMIT", CRSF_INFO},
    emptySpace
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
    lcsIdle, // step
    emptySpace
};

static struct luaItem_string luaInfo = {
    {"Bad/Good", (crsf_value_type_e)(CRSF_INFO | CRSF_FIELD_ELRS_HIDDEN)},
    emptySpace
};

static struct luaItem_string luaELRSversion = {
    {version, CRSF_INFO},
    commit
};

//---------------------------- WiFi -----------------------------
static struct luaItem_folder luaWiFiFolder = {
    {"WiFi Connectivity", CRSF_FOLDER}
};

#if defined(PLATFORM_ESP32)
static struct luaItem_command luaWebUpdate = {
    {"Enable WiFi", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};
#endif

static struct luaItem_command luaRxWebUpdate = {
    {"Enable Rx WiFi", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};

#if defined(USE_TX_BACKPACK)
static struct luaItem_command luaTxBackpackUpdate = {
    {"Enable Backpack WiFi", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};

static struct luaItem_command luaVRxBackpackUpdate = {
    {"Enable VRx WiFi", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};
#endif // USE_TX_BACKPACK
//---------------------------- WiFi -----------------------------

#if defined(PLATFORM_ESP32)
static struct luaItem_command luaBLEJoystick = {
    {"BLE Joystick", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};
#endif

//----------------------------VTX ADMINISTRATOR------------------
static struct luaItem_folder luaVtxFolder = {
    {"VTX Administrator", CRSF_FOLDER},vtxFolderDynamicName
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
    "Off;On;AUX1" LUASYM_ARROW_UP ";AUX1" LUASYM_ARROW_DN ";AUX2" LUASYM_ARROW_UP ";AUX2" LUASYM_ARROW_DN
    ";AUX3" LUASYM_ARROW_UP ";AUX3" LUASYM_ARROW_DN ";AUX4" LUASYM_ARROW_UP ";AUX4" LUASYM_ARROW_DN
    ";AUX5" LUASYM_ARROW_UP ";AUX5" LUASYM_ARROW_DN ";AUX6" LUASYM_ARROW_UP ";AUX6" LUASYM_ARROW_DN
    ";AUX7" LUASYM_ARROW_UP ";AUX7" LUASYM_ARROW_DN ";AUX8" LUASYM_ARROW_UP ";AUX8" LUASYM_ARROW_DN
    ";AUX9" LUASYM_ARROW_UP ";AUX9" LUASYM_ARROW_DN ";AUX10" LUASYM_ARROW_UP ";AUX10" LUASYM_ARROW_DN,
    emptySpace
};

static struct luaItem_command luaVtxSend = {
    {"Send VTx", CRSF_COMMAND},
    lcsIdle, // step
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

extern bool ICACHE_RAM_ATTR IsArmed();
extern TxConfig config;
extern void VtxTriggerSend();
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void SetSyncSpam();
extern void EnterBindingMode();
extern bool InBindingMode;
extern bool RxWiFiReadyToSend;
#if defined(USE_TX_BACKPACK)
extern bool TxBackpackWiFiReadyToSend;
extern bool VRxBackpackWiFiReadyToSend;
#endif
#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
extern void beginWebsever();
#endif

static uint8_t getSeparatorIndex(uint8_t index, char *searchArray)
{
  //return the separator Index + 1
  uint8_t arrayCount = 0;
  uint8_t returnvalue = 0;
  uint8_t SeparatorCount = 0;
  char *c = searchArray;
  int i = 0;
  while (c[i] != '\0')
  {
    //treat symbols as separator except : !,",#,$,%,&,',(,),*,+,,,-,.,/ as these would probably inside our label names
    if (c[i] < '!' || (c[i] > '9' && c[i] < 'A')) {
      SeparatorCount++;
      arrayCount++;
      //if found separator is equal to the nth(index) requested separator,
      //return the start of the labelSpace
      if(SeparatorCount == index+1){
        return returnvalue;
      } else {
        returnvalue = arrayCount;
      }
    } else {
      arrayCount++;
    }
    //increment the char count until null termination
    i++;
  }
  //if we reach null termination and haven't got the requested index, just return 0, which would overwrite the first label
  return returnvalue;
}
static void luadevGeneratePowerOpts()
{
  // This function modifies the strPowerLevels in place and must not
  // be called more than once!
  char *out = strPowerLevels;
  PowerLevels_e pwr = PWR_10mW;
  // Count the semicolons to move `out` to point to the MINth item
  while (pwr < MinPower)
  {
    while (*out++ != ';') ;
    pwr = (PowerLevels_e)((unsigned int)pwr + 1);
  }
  // There is no min field, compensate by shifting the index when sending/receiving
  // luaPower.min = (uint8_t)MinPower;
  luaPower.options = (const char *)out;

  // Continue until after than MAXth item and drop a null in the orginal
  // string on the semicolon (not after like the previous loop)
  while (pwr <= MaxPower)
  {
    // If out still points to a semicolon from the last loop move past it
    if (*out)
      ++out;
    while (*out && *out != ';')
      ++out;
    pwr = (PowerLevels_e)((unsigned int)pwr + 1);
  }
  *out = '\0';
}

#if defined(PLATFORM_ESP32)
static void luahandWifiBle(struct luaPropertiesCommon *item, uint8_t arg)
{
  struct luaItem_command *cmd = (struct luaItem_command *)item;
  connectionState_e targetState;
  const char *textConfirm;
  const char *textRunning;
  if ((void *)item == (void *)&luaWebUpdate)
  {
    targetState = wifiUpdate;
    textConfirm = "Enter WiFi Update?";
    textRunning = "WiFi Running...";
  }
  else
  {
    targetState = bleJoystick;
    textConfirm = "Start BLE Joystick?";
    textRunning = "Joystick Running...";
  }

  switch ((luaCmdStep_e)arg)
  {
    case lcsClick:
      if (connectionState == connected)
      {
        sendLuaCommandResponse(cmd, lcsAskConfirm, textConfirm);
        return;
      }
      // fallthrough (clicking while not connected goes right to exectute)

    case lcsConfirmed:
      sendLuaCommandResponse(cmd, lcsExecuting, textRunning);
      connectionState = targetState;
      break;

    case lcsCancel:
      sendLuaCommandResponse(cmd, lcsIdle, emptySpace);
      if (connectionState == targetState)
      {
        rebootTime = millis() + 400;
      }
      break;

    default: // LUACMDSTEP_NONE on load, LUACMDSTEP_EXECUTING (our lua) or LUACMDSTEP_QUERY (Crossfire Config)
      sendLuaCommandResponse(cmd, cmd->step, cmd->info);
      break;
  }
}
#endif

static void luahandSimpleSendCmd(struct luaPropertiesCommon *item, uint8_t arg)
{
  const char *msg = "Sending...";
  bool doExecute = arg < lcsCancel;
  if (doExecute)
  {
    if ((void *)item == (void *)&luaBind)
    {
      msg = "Binding...";
      EnterBindingMode();
    }
    else if ((void *)item == (void *)&luaVtxSend)
    {
      msg = "Sending...";
      VtxTriggerSend();
    }
    else if ((void *)item == (void *)&luaRxWebUpdate)
    {
      RxWiFiReadyToSend = true;
    }
#if defined(USE_TX_BACKPACK)
    else if ((void *)item == (void *)&luaTxBackpackUpdate)
    {
      TxBackpackWiFiReadyToSend = true;
    }
    else if ((void *)item == (void *)&luaVRxBackpackUpdate)
    {
      VRxBackpackWiFiReadyToSend = true;
    }
#endif

    sendLuaCommandResponse((struct luaItem_command *)item, lcsExecuting, msg);
  } /* if doExecute */
  else
  {
    sendLuaCommandResponse((struct luaItem_command *)item, lcsIdle, emptySpace);
  }
}

static void updateFolderName(){
  
  //power folder name
  uint8_t txPwrDyn = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  uint8_t pwrFolderLabelOffset = getSeparatorIndex(2,pwrFolderDynamicName); //4 = 3 spaces, 1 colon
  pwrFolderDynamicName[pwrFolderLabelOffset++] = '(';
  pwrFolderLabelOffset += findLuaSelectionLabel(&luaPower, &pwrFolderDynamicName[pwrFolderLabelOffset], config.GetPower() - MinPower);
  if(txPwrDyn){
    pwrFolderDynamicName[pwrFolderLabelOffset++] = folderNameSeparator[0];
    pwrFolderLabelOffset += findLuaSelectionLabel(&luaDynamicPower, &pwrFolderDynamicName[pwrFolderLabelOffset], txPwrDyn);
  }
  pwrFolderDynamicName[pwrFolderLabelOffset++] = ')';
  pwrFolderDynamicName[pwrFolderLabelOffset] = '\0';
  //vtx folder
  uint8_t vtxBand = config.GetVtxBand();
  if(vtxBand){
    luaVtxFolder.dyn_name = vtxFolderDynamicName;
    uint8_t vtxFolderLabelOffset = getSeparatorIndex(2,vtxFolderDynamicName); // 3= 2 spaces, 1 colon
    vtxFolderDynamicName[vtxFolderLabelOffset++] = '(';
    vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxBand, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxBand);
    vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
    vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxChannel, &vtxFolderDynamicName[vtxFolderLabelOffset], config.GetVtxChannel());
    uint8_t vtxPwr = config.GetVtxPower();
    //if power is no-change (-), don't show, also hide pitmode
    if(vtxPwr){
      vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
      vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxPwr, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxPwr);
      
      uint8_t vtxPit = config.GetVtxPitmode();
      //if pitmode is off, don't show
      //show pitmode AuxSwitch or show P if not OFF
      if(vtxPit != 0){
        if(vtxPit != 1){
        vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
        vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxPit, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxPit);
        } else {
        vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
        vtxFolderDynamicName[vtxFolderLabelOffset++] = 'P';
        }
      }
    }
    vtxFolderDynamicName[vtxFolderLabelOffset++] = ')';
    vtxFolderDynamicName[vtxFolderLabelOffset] = '\0';
  } else {
  //don't show vtx settings if band is OFF
    luaVtxFolder.dyn_name = NULL;
  }
}

static void registerLuaParameters()
{
  registerLUAParameter(&luaAirRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
    if ((arg < RATE_MAX) && (arg >= 0))
    {
      uint8_t rate = RATE_MAX - 1 - arg;
      rate = adjustPacketRateForBaud(rate);
      config.SetRate(rate);
    }
  });
  registerLUAParameter(&luaTlmRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
    if ((arg <= (uint8_t)TLM_RATIO_1_2) && (arg >= (uint8_t)TLM_RATIO_NO_TLM))
    {
      config.SetTlm((expresslrs_tlm_ratio_e)arg);
    }
  });
  #if defined(TARGET_TX_FM30)
  registerLUAParameter(&luaBluetoothTelem, [](struct luaPropertiesCommon *item, uint8_t arg) {
    digitalWrite(GPIO_PIN_BLUETOOTH_EN, !arg);
    devicesTriggerEvent();
  });
  #endif
  registerLUAParameter(&luaSwitch, [](struct luaPropertiesCommon *item, uint8_t arg) {
    // Only allow changing switch mode when disconnected since we need to guarantee
    // the pack and unpack functions are matched
    if (connectionState == disconnected)
    {
      // +1 to the mode because 1-bit was mode 0 and has been removed
      // The modes should be updated for 1.1RC so mode 0 can be smHybrid
      uint32_t newSwitchMode = (arg + 1) & 0b11;
      config.SetSwitchMode(newSwitchMode);
      OtaSetSwitchMode((OtaSwitchMode_e)newSwitchMode);
    }
    else
      setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
  });
  registerLUAParameter(&luaModelMatch, [](struct luaPropertiesCommon *item, uint8_t arg) {
    bool newModelMatch = arg;
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

  // POWER folder
  registerLUAParameter(&luaPowerFolder);
  luadevGeneratePowerOpts();
  registerLUAParameter(&luaPower, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetPower((PowerLevels_e)constrain(arg + MinPower, MinPower, MaxPower));
    updateFolderName();
  }, luaPowerFolder.common.id);
  registerLUAParameter(&luaDynamicPower, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetDynamicPower(arg > 0);
    config.SetBoostChannel((arg - 1) > 0 ? arg - 1 : 0);
    updateFolderName();
  }, luaPowerFolder.common.id);
#if defined(GPIO_PIN_FAN_EN)
  registerLUAParameter(&luaFanThreshold, [](struct luaPropertiesCommon *item, uint8_t arg){
    config.SetPowerFanThreshold(arg);
  }, luaPowerFolder.common.id);
#endif
#if defined(Regulatory_Domain_EU_CE_2400)
  registerLUAParameter(&luaCELimit, NULL, luaPowerFolder.common.id);
#endif
  // VTX folder
  registerLUAParameter(&luaVtxFolder);
  registerLUAParameter(&luaVtxBand, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetVtxBand(arg);
    updateFolderName();
  }, luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxChannel, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetVtxChannel(arg);
    updateFolderName();
  }, luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxPwr, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetVtxPower(arg);
    updateFolderName();
  }, luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxPit, [](struct luaPropertiesCommon *item, uint8_t arg) {
    config.SetVtxPitmode(arg);
    updateFolderName();
  }, luaVtxFolder.common.id);
  registerLUAParameter(&luaVtxSend, &luahandSimpleSendCmd, luaVtxFolder.common.id);
  // WIFI folder
  registerLUAParameter(&luaWiFiFolder);
  #if defined(PLATFORM_ESP32)
  registerLUAParameter(&luaWebUpdate, &luahandWifiBle, luaWiFiFolder.common.id);
  #endif
  registerLUAParameter(&luaRxWebUpdate, &luahandSimpleSendCmd,luaWiFiFolder.common.id);
  #if defined(USE_TX_BACKPACK)
  registerLUAParameter(&luaTxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
  registerLUAParameter(&luaVRxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
  #endif // USE_TX_BACKPACK

  #if defined(PLATFORM_ESP32)
  registerLUAParameter(&luaBLEJoystick, &luahandWifiBle);
  #endif

  registerLUAParameter(&luaBind, &luahandSimpleSendCmd);

  registerLUAParameter(&luaInfo);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event()
{
  uint8_t rate = adjustPacketRateForBaud(config.GetRate());
  setLuaTextSelectionValue(&luaAirRate, RATE_MAX - 1 - rate);
  setLuaTextSelectionValue(&luaTlmRate, config.GetTlm());
  setLuaTextSelectionValue(&luaSwitch, (uint8_t)(config.GetSwitchMode() - 1)); // -1 for missing sm1Bit
  setLuaTextSelectionValue(&luaModelMatch, (uint8_t)config.GetModelMatch());
  setLuaTextSelectionValue(&luaPower, config.GetPower() - MinPower);
#if defined(GPIO_PIN_FAN_EN)
  setLuaTextSelectionValue(&luaFanThreshold, config.GetPowerFanThreshold());
#endif

  uint8_t dynamic = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  setLuaTextSelectionValue(&luaDynamicPower, dynamic);

  setLuaTextSelectionValue(&luaVtxBand, config.GetVtxBand());
  setLuaTextSelectionValue(&luaVtxChannel, config.GetVtxChannel());
  setLuaTextSelectionValue(&luaVtxPwr, config.GetVtxPower());
  setLuaTextSelectionValue(&luaVtxPit, config.GetVtxPitmode());
  #if defined(TARGET_TX_FM30)
    setLuaTextSelectionValue(&luaBluetoothTelem, !digitalRead(GPIO_PIN_BLUETOOTH_EN));
  #endif
  return DURATION_IMMEDIATELY;
}

static int timeout()
{
  if (luaHandleUpdateParameter())
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
  updateFolderName();
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
