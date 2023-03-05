#ifdef TARGET_TX

#include "rxtx_devLua.h"
#include "CRSF.h"
#include "logging.h"
#include "OTA.h"
#include "FHSS.h"

#define STR_LUA_ALLAUX_UPDOWN  "AUX1" LUASYM_ARROW_UP ";AUX1" LUASYM_ARROW_DN ";AUX2" LUASYM_ARROW_UP ";AUX2" LUASYM_ARROW_DN \
                               ";AUX3" LUASYM_ARROW_UP ";AUX3" LUASYM_ARROW_DN ";AUX4" LUASYM_ARROW_UP ";AUX4" LUASYM_ARROW_DN \
                               ";AUX5" LUASYM_ARROW_UP ";AUX5" LUASYM_ARROW_DN ";AUX6" LUASYM_ARROW_UP ";AUX6" LUASYM_ARROW_DN \
                               ";AUX7" LUASYM_ARROW_UP ";AUX7" LUASYM_ARROW_DN ";AUX8" LUASYM_ARROW_UP ";AUX8" LUASYM_ARROW_DN \
                               ";AUX9" LUASYM_ARROW_UP ";AUX9" LUASYM_ARROW_DN ";AUX10" LUASYM_ARROW_UP ";AUX10" LUASYM_ARROW_DN

extern char backpackVersion[];

static char version_domain[20+1+6+1];
char pwrFolderDynamicName[] = "TX Power (1000 Dynamic)";
char vtxFolderDynamicName[] = "VTX Admin (OFF:C:1 Aux11 )";
static char modelMatchUnit[] = " (ID: 00)";
static char tlmBandwidth[] = " (xxxxxbps)";
static const char folderNameSeparator[2] = {' ',':'};
static const char switchmodeOpts4ch[] = "Wide;Hybrid";
static const char switchmodeOpts8ch[] = "8ch;16ch Rate/2;12ch Mixed";
static const char antennamodeOpts[] = "Gemini;Ant 1;Ant 2;Switch";
static const char luastrDvrAux[] = "Off;" STR_LUA_ALLAUX_UPDOWN;
static const char luastrDvrDelay[] = "0s;5s;15s;30s;45s;1min;2min";
static const char luastrDisabled[] = "Disabled";

#define HAS_RADIO (GPIO_PIN_SCK != UNDEF_PIN)

static struct luaItem_selection luaAirRate = {
    {"Packet Rate", CRSF_TEXT_SELECTION},
    0, // value
    STR_LUA_PACKETRATES,
    STR_EMPTYSPACE
};

static struct luaItem_selection luaTlmRate = {
    {"Telem Ratio", CRSF_TEXT_SELECTION},
    0, // value
    "Std;Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2;Race",
    tlmBandwidth
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
    STR_EMPTYSPACE
};

#if defined(GPIO_PIN_FAN_EN) || defined(GPIO_PIN_FAN_PWM)
static struct luaItem_selection luaFanThreshold = {
    {"Fan Thresh", CRSF_TEXT_SELECTION},
    0, // value
    "10mW;25mW;50mW;100mW;250mW;500mW;1000mW;2000mW;Never",
    STR_EMPTYSPACE // units embedded so it won't display "NevermW"
};
#endif

#if defined(Regulatory_Domain_EU_CE_2400)
static struct luaItem_string luaCELimit = {
    {"100mW CE LIMIT", CRSF_INFO},
    STR_EMPTYSPACE
};
#endif

//----------------------------POWER------------------

static struct luaItem_selection luaSwitch = {
    {"Switch Mode", CRSF_TEXT_SELECTION},
    0, // value
    switchmodeOpts4ch,
    STR_EMPTYSPACE
};

#if defined(GPIO_PIN_NSS_2)
  static struct luaItem_selection luaAntenna = {
      {"Antenna Mode", CRSF_TEXT_SELECTION},
      0, // value
      antennamodeOpts,
      STR_EMPTYSPACE
  };
#endif

static struct luaItem_selection luaModelMatch = {
    {"Model Match", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    modelMatchUnit
};

static struct luaItem_command luaBind = {
    {"Bind", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static struct luaItem_string luaInfo = {
    {"Bad/Good", (crsf_value_type_e)(CRSF_INFO | CRSF_FIELD_ELRS_HIDDEN)},
    STR_EMPTYSPACE
};

static struct luaItem_string luaELRSversion = {
    {version_domain, CRSF_INFO},
    commit
};

//---------------------------- WiFi -----------------------------
static struct luaItem_folder luaWiFiFolder = {
    {"WiFi Connectivity", CRSF_FOLDER}
};

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
static struct luaItem_command luaWebUpdate = {
    {"Enable WiFi", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};
#endif

static struct luaItem_command luaRxWebUpdate = {
    {"Enable Rx WiFi", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static struct luaItem_command luaTxBackpackUpdate = {
    {"Enable Backpack WiFi", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static struct luaItem_command luaVRxBackpackUpdate = {
    {"Enable VRx WiFi", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};
//---------------------------- WiFi -----------------------------

#if defined(PLATFORM_ESP32)
static struct luaItem_command luaBLEJoystick = {
    {"BLE Joystick", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
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
    STR_EMPTYSPACE
};

static struct luaItem_selection luaVtxChannel = {
    {"Channel", CRSF_TEXT_SELECTION},
    0, // value
    "1;2;3;4;5;6;7;8",
    STR_EMPTYSPACE
};

static struct luaItem_selection luaVtxPwr = {
    {"Pwr Lvl", CRSF_TEXT_SELECTION},
    0, // value
    "-;1;2;3;4;5;6;7;8",
    STR_EMPTYSPACE
};

static struct luaItem_selection luaVtxPit = {
    {"Pitmode", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On;" STR_LUA_ALLAUX_UPDOWN,
    STR_EMPTYSPACE
};

static struct luaItem_command luaVtxSend = {
    {"Send VTx", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};
//----------------------------VTX ADMINISTRATOR------------------

#if defined(TARGET_TX_FM30)
struct luaItem_selection luaBluetoothTelem = {
    {"BT Telemetry", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    STR_EMPTYSPACE
};
#endif

//---------------------------- BACKPACK ------------------
static struct luaItem_folder luaBackpackFolder = {
    {"Backpack", CRSF_FOLDER},
};

#if defined(GPIO_PIN_BACKPACK_EN)
static struct luaItem_selection luaBackpackEnable = {
    {"Backpack", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    STR_EMPTYSPACE};
#endif

static struct luaItem_selection luaDvrAux = {
    {"DVR Rec", CRSF_TEXT_SELECTION},
    0, // value
    luastrDvrAux,
    STR_EMPTYSPACE};

static struct luaItem_selection luaDvrStartDelay = {
    {"DVR Srt Dly", CRSF_TEXT_SELECTION},
    0, // value
    luastrDvrDelay,
    STR_EMPTYSPACE};

static struct luaItem_selection luaDvrStopDelay = {
    {"DVR Stp Dly", CRSF_TEXT_SELECTION},
    0, // value
    luastrDvrDelay,
    STR_EMPTYSPACE};

static struct luaItem_string luaBackpackVersion = {
    {"Version", CRSF_INFO},
    backpackVersion};

//---------------------------- BACKPACK ------------------

static char luaBadGoodString[10];
static int event();

extern TxConfig config;
extern void VtxTriggerSend();
extern void ResetPower();
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void SetSyncSpam();
extern void EnterBindingMode();
extern bool InBindingMode;
extern bool RxWiFiReadyToSend;
#if defined(USE_TX_BACKPACK)
extern bool TxBackpackWiFiReadyToSend;
extern bool VRxBackpackWiFiReadyToSend;
#endif
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern unsigned long rebootTime;
extern void setWifiUpdateMode();
#endif

static void luadevUpdateModelID() {
  itoa(CRSF::getModelID(), modelMatchUnit+6, 10);
  strcat(modelMatchUnit, ")");
}

static void luadevUpdateTlmBandwidth()
{
  expresslrs_tlm_ratio_e eRatio = (expresslrs_tlm_ratio_e)config.GetTlm();
  // TLM_RATIO_STD / TLM_RATIO_DISARMED
  if (eRatio == TLM_RATIO_STD || eRatio == TLM_RATIO_DISARMED)
  {
    // For Standard ratio, display the ratio instead of bps
    strcpy(tlmBandwidth, " (1:");
    uint8_t ratioDiv = TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    itoa(ratioDiv, &tlmBandwidth[4], 10);
    strcat(tlmBandwidth, ")");
  }

  // TLM_RATIO_NO_TLM
  else if (eRatio == TLM_RATIO_NO_TLM)
  {
    tlmBandwidth[0] = '\0';
  }

  // All normal ratios
  else
  {
    tlmBandwidth[0] = ' ';

    uint16_t hz = RateEnumToHz(ExpressLRS_currAirRate_Modparams->enum_rate);
    uint8_t ratiodiv = TLMratioEnumToValue(eRatio);
    uint8_t burst = TLMBurstMaxForRateRatio(hz, ratiodiv);
    uint8_t bytesPerCall = OtaIsFullRes ? ELRS8_TELEMETRY_BYTES_PER_CALL : ELRS4_TELEMETRY_BYTES_PER_CALL;
    uint32_t bandwidthValue = bytesPerCall * 8U * burst * hz / ratiodiv / (burst + 1);
    if (OtaIsFullRes)
    {
      // Due to fullres also packing telemetry into the LinkStats packet, there is at least
      // N bytes more data for every rate except 100Hz 1:128, and 2*N bytes more for many
      // rates. The calculation is a more complex though, so just approximate some of the
      // extra bandwidth
      bandwidthValue += 8U * (ELRS8_TELEMETRY_BYTES_PER_CALL - sizeof(OTA_LinkStats_s));
    }

    itoa(bandwidthValue, &tlmBandwidth[2], 10);
    strcat(tlmBandwidth, "bps)");
  }
}

static void luadevUpdateBackpackOpts()
{
  if (config.GetBackpackDisable())
  {
    // If backpack is disabled, set all the Backpack select options to "Disabled"
    luaDvrAux.options = luastrDisabled;
    luaDvrStartDelay.options = luastrDisabled;
    luaDvrStopDelay.options = luastrDisabled;
  }
  else
  {
    luaDvrAux.options = luastrDvrAux;
    luaDvrStartDelay.options = luastrDvrDelay;
    luaDvrStopDelay.options = luastrDvrDelay;
  }
}

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
static void luahandWifiBle(struct luaPropertiesCommon *item, uint8_t arg)
{
  struct luaItem_command *cmd = (struct luaItem_command *)item;
  std::function<void()> setTargetState;
  connectionState_e targetState;
  const char *textConfirm;
  const char *textRunning;
  if ((void *)item == (void *)&luaWebUpdate)
  {
    setTargetState = setWifiUpdateMode;
    textConfirm = "Enter WiFi Update?";
    textRunning = "WiFi Running...";
    targetState = wifiUpdate;
  }
  else
  {
    setTargetState = []() {
      connectionState = bleJoystick;
    };
    textConfirm = "Start BLE Joystick?";
    textRunning = "Joystick Running...";
    targetState = bleJoystick;
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
      setTargetState();
      break;

    case lcsCancel:
      sendLuaCommandResponse(cmd, lcsIdle, STR_EMPTYSPACE);
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
  static uint32_t lastLcsPoll;
  if (arg < lcsCancel)
  {
    lastLcsPoll = millis();
    if ((void *)item == (void *)&luaBind)
    {
      msg = "Binding...";
      EnterBindingMode();
    }
    else if ((void *)item == (void *)&luaVtxSend)
    {
      VtxTriggerSend();
    }
    else if ((void *)item == (void *)&luaRxWebUpdate)
    {
      RxWiFiReadyToSend = true;
    }
#if defined(USE_TX_BACKPACK)
    else if ((void *)item == (void *)&luaTxBackpackUpdate && OPT_USE_TX_BACKPACK)
    {
      TxBackpackWiFiReadyToSend = true;
    }
    else if ((void *)item == (void *)&luaVRxBackpackUpdate && OPT_USE_TX_BACKPACK)
    {
      VRxBackpackWiFiReadyToSend = true;
    }
#endif
    sendLuaCommandResponse((struct luaItem_command *)item, lcsExecuting, msg);
  } /* if doExecute */
  else if(arg == lcsCancel || ((millis() - lastLcsPoll)> 2000))
  {
    sendLuaCommandResponse((struct luaItem_command *)item, lcsIdle, STR_EMPTYSPACE);
  }
}

static void updateFolderName_TxPower()
{
  uint8_t txPwrDyn = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  uint8_t pwrFolderLabelOffset = 10; // start writing after "TX Power ("

  // Power Level
  pwrFolderLabelOffset += findLuaSelectionLabel(&luaPower, &pwrFolderDynamicName[pwrFolderLabelOffset], config.GetPower() - MinPower);

  // Dynamic Power
  if (txPwrDyn)
  {
    pwrFolderDynamicName[pwrFolderLabelOffset++] = folderNameSeparator[0];
    pwrFolderLabelOffset += findLuaSelectionLabel(&luaDynamicPower, &pwrFolderDynamicName[pwrFolderLabelOffset], txPwrDyn);
  }

  pwrFolderDynamicName[pwrFolderLabelOffset++] = ')';
  pwrFolderDynamicName[pwrFolderLabelOffset] = '\0';
}

static void updateFolderName_VtxAdmin()
{
  uint8_t vtxBand = config.GetVtxBand();
  if (vtxBand)
  {
    luaVtxFolder.dyn_name = vtxFolderDynamicName;
    uint8_t vtxFolderLabelOffset = 11; // start writing after "VTX Admin ("

    // Band
    vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxBand, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxBand);
    vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];

    // Channel
    vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxChannel, &vtxFolderDynamicName[vtxFolderLabelOffset], config.GetVtxChannel());

    // VTX Power
    uint8_t vtxPwr = config.GetVtxPower();
    //if power is no-change (-), don't show, also hide pitmode
    if (vtxPwr)
    {
      vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
      vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxPwr, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxPwr);

      // Pit Mode
      uint8_t vtxPit = config.GetVtxPitmode();
      //if pitmode is off, don't show
      //show pitmode AuxSwitch or show P if not OFF
      if (vtxPit != 0)
      {
        if (vtxPit != 1)
        {
          vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
          vtxFolderLabelOffset += findLuaSelectionLabel(&luaVtxPit, &vtxFolderDynamicName[vtxFolderLabelOffset], vtxPit);
        }
        else
        {
          vtxFolderDynamicName[vtxFolderLabelOffset++] = folderNameSeparator[1];
          vtxFolderDynamicName[vtxFolderLabelOffset++] = 'P';
        }
      }
    }
    vtxFolderDynamicName[vtxFolderLabelOffset++] = ')';
    vtxFolderDynamicName[vtxFolderLabelOffset] = '\0';
  }
  else
  {
    //don't show vtx settings if band is OFF
    luaVtxFolder.dyn_name = NULL;
  }
}

/***
 * @brief: Update the luaBadGoodString with the current bad/good count
 * This item is hidden on our Lua and only displayed in other systems that don't poll our status
 * Called from luaRegisterDevicePingCallback
 ****/
static void luadevUpdateBadGood()
{
  itoa(CRSF::BadPktsCountResult, luaBadGoodString, 10);
  strcat(luaBadGoodString, "/");
  itoa(CRSF::GoodPktsCountResult, luaBadGoodString + strlen(luaBadGoodString), 10);
}

/***
 * @brief: Update the dynamic strings used for folder names and labels
 ***/
void luadevUpdateFolderNames()
{
  updateFolderName_TxPower();
  updateFolderName_VtxAdmin();

  // These aren't folder names, just string labels slapped in the units field generally
  luadevUpdateTlmBandwidth();
  luadevUpdateBackpackOpts();
}

uint8_t adjustSwitchModeForAirRate(OtaSwitchMode_e eSwitchMode, uint8_t packetSize)
{
  // Only the fullres modes have 3 switch modes, so reset the switch mode if outside the
  // range for 4ch mode
  if (packetSize == OTA4_PACKET_SIZE)
  {
    if (eSwitchMode > smHybridOr16ch)
      return smWideOr8ch;
  }

  return eSwitchMode;
}

static void registerLuaParameters()
{
  if (HAS_RADIO) {
    registerLUAParameter(&luaAirRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
    if (arg < RATE_MAX)
    {
      uint8_t selectedRate = RATE_MAX - 1 - arg;
      uint8_t actualRate = adjustPacketRateForBaud(selectedRate);
      uint8_t newSwitchMode = adjustSwitchModeForAirRate(
        (OtaSwitchMode_e)config.GetSwitchMode(), get_elrs_airRateConfig(actualRate)->PayloadLength);
      // If the switch mode is going to change, block the change while connected
      if (newSwitchMode == OtaSwitchModeCurrent || connectionState == disconnected)
      {
        config.SetRate(actualRate);
        config.SetSwitchMode(newSwitchMode);
        if (actualRate != selectedRate)
        {
          setLuaWarningFlag(LUA_FLAG_ERROR_BAUDRATE, true);
        }
      }
      else
        setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
    }
    });
    registerLUAParameter(&luaTlmRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
      expresslrs_tlm_ratio_e eRatio = (expresslrs_tlm_ratio_e)arg;
      if (eRatio <= TLM_RATIO_DISARMED)
      {
        // Don't allow TLM ratio changes if using AIRPORT
        if (!firmwareOptions.is_airport)
        {
          config.SetTlm(eRatio);
        }
      }
    });
    #if defined(TARGET_TX_FM30)
    registerLUAParameter(&luaBluetoothTelem, [](struct luaPropertiesCommon *item, uint8_t arg) {
      digitalWrite(GPIO_PIN_BLUETOOTH_EN, !arg);
      devicesTriggerEvent();
    });
    #endif
    if (!firmwareOptions.is_airport)
    {
      registerLUAParameter(&luaSwitch, [](struct luaPropertiesCommon *item, uint8_t arg) {
        // Only allow changing switch mode when disconnected since we need to guarantee
        // the pack and unpack functions are matched
        if (connectionState == disconnected)
        {
          config.SetSwitchMode(arg);
          OtaUpdateSerializers((OtaSwitchMode_e)arg, ExpressLRS_currAirRate_Modparams->PayloadLength);
        }
        else
          setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
      });
      if (isDualRadio())
      {
        registerLUAParameter(&luaAntenna, [](struct luaPropertiesCommon *item, uint8_t arg) {
          config.SetAntennaMode(arg);
        });
      }
      registerLUAParameter(&luaModelMatch, [](struct luaPropertiesCommon *item, uint8_t arg) {
        bool newModelMatch = arg;
        config.SetModelMatch(newModelMatch);
        if (connectionState == connected)
        {
          mspPacket_t msp;
          msp.reset();
          msp.makeCommand();
          msp.function = MSP_SET_RX_CONFIG;
          msp.addByte(MSP_ELRS_MODEL_ID);
          msp.addByte(newModelMatch ? CRSF::getModelID() : 0xff);
          CRSF::AddMspMessage(&msp);
        }
        luadevUpdateModelID();
      });
    }

    // POWER folder
    registerLUAParameter(&luaPowerFolder);
    luadevGeneratePowerOpts(&luaPower);
    registerLUAParameter(&luaPower, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetPower((PowerLevels_e)constrain(arg + POWERMGNT::getMinPower(), POWERMGNT::getMinPower(), POWERMGNT::getMaxPower()));
      if (!config.IsModified())
      {
          ResetPower();
      }
    }, luaPowerFolder.common.id);
    registerLUAParameter(&luaDynamicPower, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetDynamicPower(arg > 0);
      config.SetBoostChannel((arg - 1) > 0 ? arg - 1 : 0);
    }, luaPowerFolder.common.id);
  }
  if (GPIO_PIN_FAN_EN != UNDEF_PIN || GPIO_PIN_FAN_PWM != UNDEF_PIN) {
    registerLUAParameter(&luaFanThreshold, [](struct luaPropertiesCommon *item, uint8_t arg){
      config.SetPowerFanThreshold(arg);
    }, luaPowerFolder.common.id);
  }
#if defined(Regulatory_Domain_EU_CE_2400)
  if (HAS_RADIO) {
    registerLUAParameter(&luaCELimit, NULL, luaPowerFolder.common.id);
  }
#endif
  if ((HAS_RADIO || OPT_USE_TX_BACKPACK) && !firmwareOptions.is_airport) {
    // VTX folder
    registerLUAParameter(&luaVtxFolder);
    registerLUAParameter(&luaVtxBand, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetVtxBand(arg);
    }, luaVtxFolder.common.id);
    registerLUAParameter(&luaVtxChannel, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetVtxChannel(arg);
    }, luaVtxFolder.common.id);
    registerLUAParameter(&luaVtxPwr, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetVtxPower(arg);
    }, luaVtxFolder.common.id);
    registerLUAParameter(&luaVtxPit, [](struct luaPropertiesCommon *item, uint8_t arg) {
      config.SetVtxPitmode(arg);
    }, luaVtxFolder.common.id);
    registerLUAParameter(&luaVtxSend, &luahandSimpleSendCmd, luaVtxFolder.common.id);
  }

  // WIFI folder
  #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
  registerLUAParameter(&luaWiFiFolder);
  registerLUAParameter(&luaWebUpdate, &luahandWifiBle, luaWiFiFolder.common.id);
  #else
  if (HAS_RADIO || OPT_USE_TX_BACKPACK) {
    registerLUAParameter(&luaWiFiFolder);
  }
  #endif
  if (HAS_RADIO) {
    registerLUAParameter(&luaRxWebUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);

    if (OPT_USE_TX_BACKPACK) {
      registerLUAParameter(&luaTxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
      registerLUAParameter(&luaVRxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
      // Backpack folder
      registerLUAParameter(&luaBackpackFolder);
      #if defined(GPIO_PIN_BACKPACK_EN)
      if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
      {
        registerLUAParameter(
            &luaBackpackEnable, [](luaPropertiesCommon *item, uint8_t arg) {
                // option is Off/On (enable) and config storage is On/Off (disable)
                config.SetBackpackDisable(arg == 0);
            }, luaBackpackFolder.common.id);
      }
      #endif
      registerLUAParameter(
          &luaDvrAux, [](luaPropertiesCommon *item, uint8_t arg) {
              if (config.GetBackpackDisable() == false)
                config.SetDvrAux(arg);
          },
          luaBackpackFolder.common.id);
      registerLUAParameter(
          &luaDvrStartDelay, [](luaPropertiesCommon *item, uint8_t arg) {
              if (config.GetBackpackDisable() == false)
                config.SetDvrStartDelay(arg);
          },
          luaBackpackFolder.common.id);
      registerLUAParameter(
          &luaDvrStopDelay, [](luaPropertiesCommon *item, uint8_t arg) {
            if (config.GetBackpackDisable() == false)
              config.SetDvrStopDelay(arg);
          },
          luaBackpackFolder.common.id);
      registerLUAParameter(&luaBackpackVersion, nullptr, luaBackpackFolder.common.id);
    }
  }

  #if defined(PLATFORM_ESP32)
  registerLUAParameter(&luaBLEJoystick, &luahandWifiBle);
  #endif

  if (HAS_RADIO) {
    registerLUAParameter(&luaBind, &luahandSimpleSendCmd);
  }

  registerLUAParameter(&luaInfo);
  if (strlen(version) < 21) {
    strlcpy(version_domain, version, 21);
    strlcat(version_domain, " ", sizeof(version_domain));
  } else {
    strlcpy(version_domain, version, 18);
    strlcat(version_domain, "... ", sizeof(version_domain));
  }
  strlcat(version_domain, FHSSconfig->domain, sizeof(version_domain));
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event()
{
  if (connectionState > FAILURE_STATES)
  {
    return DURATION_NEVER;
  }
  uint8_t currentRate = adjustPacketRateForBaud(config.GetRate());
  setLuaTextSelectionValue(&luaAirRate, RATE_MAX - 1 - currentRate);
  setLuaTextSelectionValue(&luaTlmRate, config.GetTlm());
  setLuaTextSelectionValue(&luaSwitch, config.GetSwitchMode());
  luaSwitch.options = OtaIsFullRes ? switchmodeOpts8ch : switchmodeOpts4ch;
  if (isDualRadio())
  {
    setLuaTextSelectionValue(&luaAntenna, config.GetAntennaMode());
  }
  luadevUpdateModelID();
  setLuaTextSelectionValue(&luaModelMatch, (uint8_t)config.GetModelMatch());
  setLuaTextSelectionValue(&luaPower, config.GetPower() - MinPower);
  if (GPIO_PIN_FAN_EN != UNDEF_PIN || GPIO_PIN_FAN_PWM != UNDEF_PIN)
  {
    setLuaTextSelectionValue(&luaFanThreshold, config.GetPowerFanThreshold());
  }

  uint8_t dynamic = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  setLuaTextSelectionValue(&luaDynamicPower, dynamic);

  setLuaTextSelectionValue(&luaVtxBand, config.GetVtxBand());
  setLuaTextSelectionValue(&luaVtxChannel, config.GetVtxChannel());
  setLuaTextSelectionValue(&luaVtxPwr, config.GetVtxPower());
  setLuaTextSelectionValue(&luaVtxPit, config.GetVtxPitmode());
  if (OPT_USE_TX_BACKPACK)
  {
#if defined(GPIO_PIN_BACKPACK_EN)
    setLuaTextSelectionValue(&luaBackpackEnable, config.GetBackpackDisable() ? 0 : 1);
#endif
    setLuaTextSelectionValue(&luaDvrAux, config.GetBackpackDisable() ? 0 : config.GetDvrAux());
    setLuaTextSelectionValue(&luaDvrStartDelay, config.GetBackpackDisable() ? 0 : config.GetDvrStartDelay());
    setLuaTextSelectionValue(&luaDvrStopDelay, config.GetBackpackDisable() ? 0 : config.GetDvrStopDelay());
    setLuaStringValue(&luaBackpackVersion, backpackVersion);
  }
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
  if (connectionState > FAILURE_STATES)
  {
    return DURATION_NEVER;
  }
  CRSF::RecvParameterUpdate = &luaParamUpdateReq;
  registerLuaParameters();

  setLuaStringValue(&luaInfo, luaBadGoodString);
  luaRegisterDevicePingCallback(&luadevUpdateBadGood);

  luadevUpdateFolderNames();
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
