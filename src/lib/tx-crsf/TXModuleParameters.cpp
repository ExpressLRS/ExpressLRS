#include "TXModuleEndpoint.h"

#include "CRSFHandset.h"
#include "CRSFRouter.h"
#include "FHSS.h"
#include "OTA.h"
#include "POWERMGNT.h"
#include "config.h"
#include "helpers.h"
#include "msptypes.h"
#include "rxtx_devLua.h"

uint8_t adjustSwitchModeForAirRate(OtaSwitchMode_e eSwitchMode, uint8_t packetSize);

#define STR_LUA_ALLAUX         "AUX1;AUX2;AUX3;AUX4;AUX5;AUX6;AUX7;AUX8;AUX9;AUX10"

#define STR_LUA_ALLAUX_UPDOWN  "AUX1" LUASYM_ARROW_UP ";AUX1" LUASYM_ARROW_DN ";AUX2" LUASYM_ARROW_UP ";AUX2" LUASYM_ARROW_DN \
                               ";AUX3" LUASYM_ARROW_UP ";AUX3" LUASYM_ARROW_DN ";AUX4" LUASYM_ARROW_UP ";AUX4" LUASYM_ARROW_DN \
                               ";AUX5" LUASYM_ARROW_UP ";AUX5" LUASYM_ARROW_DN ";AUX6" LUASYM_ARROW_UP ";AUX6" LUASYM_ARROW_DN \
                               ";AUX7" LUASYM_ARROW_UP ";AUX7" LUASYM_ARROW_DN ";AUX8" LUASYM_ARROW_UP ";AUX8" LUASYM_ARROW_DN \
                               ";AUX9" LUASYM_ARROW_UP ";AUX9" LUASYM_ARROW_DN ";AUX10" LUASYM_ARROW_UP ";AUX10" LUASYM_ARROW_DN

#if defined(RADIO_SX127X)
#define STR_LUA_PACKETRATES \
    "D50Hz(-112dBm);25Hz(-123dBm);50Hz(-120dBm);100Hz(-117dBm);100Hz Full(-112dBm);200Hz(-112dBm)"
#elif defined(RADIO_LR1121)
#define STR_LUA_PACKETRATES \
    "100Hz Full(-112dBm);150Hz(-112dBm);" \
    "50Hz(-115dBm);100Hz Full(-112dBm);150Hz(-112dBm);250Hz(-108dBm);333Hz Full(-105dBm);500Hz(-105dBm);" \
    "DK250(-103dBm);DK500(-103dBm);K1000(-103dBm);" \
    "D50Hz(-112dBm);25Hz(-123dBm);50Hz(-120dBm);100Hz(-117dBm);100Hz Full(-112dBm);200Hz(-112dBm);200Hz Full(-111dBm);250Hz(-111dBm);" \
    "K1000 Full(-101dBm)"
#elif defined(RADIO_SX128X)
#define STR_LUA_PACKETRATES \
    "50Hz(-115dBm);100Hz Full(-112dBm);150Hz(-112dBm);250Hz(-108dBm);333Hz Full(-105dBm);500Hz(-105dBm);" \
    "D250(-104dBm);D500(-104dBm);F500(-104dBm);F1000(-104dBm)"
#else
#error Invalid radio configuration!
#endif

#define HAS_RADIO (GPIO_PIN_SCK != UNDEF_PIN)

extern char backpackVersion[];
extern TXModuleEndpoint crsfTransmitter;

static char version_domain[20+1+6+1];
char pwrFolderDynamicName[] = "TX Power (1000 Dynamic)";
char vtxFolderDynamicName[] = "VTX Admin (OFF:C:1 Aux11 )";
static char modelMatchUnit[] = " (ID: 00)";
static char tlmBandwidth[] = " (xxxxxbps)";
static const char folderNameSeparator[2] = {' ',':'};
static const char tlmRatios[] = "Std;Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2;Race";
static const char tlmRatiosMav[] = ";;;;;;;;1:2;";
static const char switchmodeOpts4ch[] = "Wide;Hybrid";
static const char switchmodeOpts4chMav[] = ";Hybrid";
static const char switchmodeOpts8ch[] = "8ch;16ch Rate/2;12ch Mixed";
static const char switchmodeOpts8chMav[] = ";16ch Rate/2;";
static const char antennamodeOpts[] = "Gemini;Ant 1;Ant 2;Switch";
static const char antennamodeOptsDualBand[] = "Gemini;;;";
static const char linkModeOpts[] = "Normal;MAVLink";
static const char luastrDvrAux[] = "Off;" STR_LUA_ALLAUX_UPDOWN;
static const char luastrDvrDelay[] = "0s;5s;15s;30s;45s;1min;2min";
static const char luastrHeadTrackingEnable[] = "Off;On;" STR_LUA_ALLAUX_UPDOWN;
static const char luastrHeadTrackingStart[] = "EdgeTX;" STR_LUA_ALLAUX;
static const char luastrOffOn[] = "Off;On";
static char luastrPacketRates[] = STR_LUA_PACKETRATES;

static char luaBadGoodString[10];
static uint8_t luaWarningFlags = 0b00000000; //8 flag, 1 bit for each flag. set the bit to 1 to show specific warning. 3 MSB is for critical flag

#if defined(RADIO_LR1121)
static char luastrRFBands[32];
static enum RFMode : uint8_t
{
    RF_MODE_900 = 0,
    RF_MODE_2G4 = 1,
    RF_MODE_DUAL = 2,
} rfMode;

static struct luaItem_selection luaRFBand = {
    {"RF Band", CRSF_TEXT_SELECTION},
    0, // value
    luastrRFBands,
    STR_EMPTYSPACE
};
#endif

static struct luaItem_selection luaAirRate = {
    {"Packet Rate", CRSF_TEXT_SELECTION},
    0, // value
    luastrPacketRates,
    STR_EMPTYSPACE
};

static struct luaItem_selection luaTlmRate = {
    {"Telem Ratio", CRSF_TEXT_SELECTION},
    0, // value
    tlmRatios,
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

static struct luaItem_selection luaFanThreshold = {
    {"Fan Thresh", CRSF_TEXT_SELECTION},
    0, // value
    "10mW;25mW;50mW;100mW;250mW;500mW;1000mW;2000mW;Never",
    STR_EMPTYSPACE // units embedded so it won't display "NevermW"
};

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

static struct luaItem_selection luaAntenna = {
    {"Antenna Mode", CRSF_TEXT_SELECTION},
    0, // value
    antennamodeOpts,
    STR_EMPTYSPACE
};

static struct luaItem_selection luaLinkMode = {
    {"Link Mode", CRSF_TEXT_SELECTION},
    0, // value
    linkModeOpts,
    STR_EMPTYSPACE
};

static struct luaItem_selection luaModelMatch = {
    {"Model Match", CRSF_TEXT_SELECTION},
    0, // value
    luastrOffOn,
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

static struct luaItem_command luaWebUpdate = {
    {"Enable WiFi", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

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

static struct luaItem_int8 luaVtxChannel = {
    {"Channel", CRSF_UINT8},
    0, // value
    1, // min
    8, // max
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

//---------------------------- BACKPACK ------------------
static struct luaItem_folder luaBackpackFolder = {
    {"Backpack", CRSF_FOLDER},
};

static struct luaItem_selection luaBackpackEnable = {
    {"Backpack", CRSF_TEXT_SELECTION},
    0, // value
    luastrOffOn,
    STR_EMPTYSPACE};

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

static struct luaItem_selection luaHeadTrackingEnableChannel = {
    {"HT Enable", CRSF_TEXT_SELECTION},
    0, // value
    luastrHeadTrackingEnable,
    STR_EMPTYSPACE};

static struct luaItem_selection luaHeadTrackingStartChannel = {
    {"HT Start Channel", CRSF_TEXT_SELECTION},
    0, // value
    luastrHeadTrackingStart,
    STR_EMPTYSPACE};

static struct luaItem_selection luaBackpackTelemetry = {
    {"Telemetry", CRSF_TEXT_SELECTION},
    0, // value
    "Off;ESPNOW;WiFi",
    STR_EMPTYSPACE};

static struct luaItem_string luaBackpackVersion = {
    {"Version", CRSF_INFO},
    backpackVersion};

//---------------------------- BACKPACK ------------------

extern TxConfig config;
extern void VtxTriggerSend();
extern void ResetPower();
extern uint8_t adjustPacketRateForBaud(uint8_t rate);
extern void SetSyncSpam();
extern bool RxWiFiReadyToSend;
extern bool BackpackTelemReadyToSend;
extern bool TxBackpackWiFiReadyToSend;
extern bool VRxBackpackWiFiReadyToSend;
extern unsigned long rebootTime;
extern void setWifiUpdateMode();


void luaSupressCriticalErrors()
{
    // clear the critical error bits of the warning flags
    luaWarningFlags &= 0b00011111;
}

/***
 * @brief: Update the luaBadGoodString with the current bad/good count
 * This item is hidden on our Lua and only displayed in other systems that don't poll our status
 ****/
static void pingCallback()
{
    luaSupressCriticalErrors();
    itoa(CRSFHandset::BadPktsCountResult, luaBadGoodString, 10);
    strcat(luaBadGoodString, "/");
    itoa(CRSFHandset::GoodPktsCountResult, luaBadGoodString + strlen(luaBadGoodString), 10);
}

void setLuaWarningFlag(lua_Flags flag, bool value)
{
  if (value)
  {
    luaWarningFlags |= 1 << (uint8_t)flag;
  }
  else
  {
    luaWarningFlags &= ~(1 << (uint8_t)flag);
  }
}

void sendELRSstatus(crsf_addr_e origin)
{
  constexpr const char *messages[] = { //higher order = higher priority
    "",                   //status2 = connected status
    "",                   //status1, reserved for future use
    "Model Mismatch",     //warning3, model mismatch
    "[ ! Armed ! ]",      //warning2, AUX1 high / armed
    "",           //warning1, reserved for future use
    "Not while connected",  //critical warning3, trying to change a protected value while connected
    "Baud rate too low",  //critical warning2, changing packet rate and baud rate too low
    ""   //critical warning1, reserved for future use
  };
  auto warningInfo = "";

  for (int i = 7; i >= 0; i--)
  {
      if (luaWarningFlags & (1 << i))
      {
          warningInfo = messages[i];
          break;
      }
  }
  const uint8_t payloadSize = sizeof(tagLuaElrsParams) + strlen(warningInfo) + 1;
  uint8_t buffer[sizeof(crsf_ext_header_t) + payloadSize + 1];
  const auto params = (struct tagLuaElrsParams *)&buffer[sizeof(crsf_ext_header_t)];

  setLuaWarningFlag(LUA_FLAG_MODEL_MATCH, connectionState == connected && connectionHasModelMatch == false);
  setLuaWarningFlag(LUA_FLAG_CONNECTED, connectionState == connected);
  setLuaWarningFlag(LUA_FLAG_ISARMED, handset->IsArmed());

  params->pktsBad = CRSFHandset::BadPktsCountResult;
  params->pktsGood = htobe16(CRSFHandset::GoodPktsCountResult);
  params->flags = luaWarningFlags;
  // to support sending a params.msg, buffer should be extended by the strlen of the message
  // and copied into params->msg (with trailing null)
  strcpy(params->msg, warningInfo);
  crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)buffer, CRSF_FRAMETYPE_ELRS_STATUS, CRSF_EXT_FRAME_SIZE(payloadSize), origin, CRSF_ADDRESS_CRSF_TRANSMITTER);
  crsfRouter.processMessage(nullptr, (crsf_header_t *)buffer);
}

static void luadevUpdateModelID() {
  itoa(crsfTransmitter.modelId, modelMatchUnit+6, 10);
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

    uint16_t hz = 1000000 / ExpressLRS_currAirRate_Modparams->interval;
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
    LUA_FIELD_HIDE(luaDvrAux);
    LUA_FIELD_HIDE(luaDvrStartDelay);
    LUA_FIELD_HIDE(luaDvrStopDelay);
    LUA_FIELD_HIDE(luaHeadTrackingEnableChannel);
    LUA_FIELD_HIDE(luaHeadTrackingStartChannel);
    LUA_FIELD_HIDE(luaBackpackTelemetry);
    LUA_FIELD_HIDE(luaBackpackVersion);
  }
  else
  {
    LUA_FIELD_SHOW(luaDvrAux);
    LUA_FIELD_SHOW(luaDvrStartDelay);
    LUA_FIELD_SHOW(luaDvrStopDelay);
    LUA_FIELD_SHOW(luaHeadTrackingEnableChannel);
    LUA_FIELD_SHOW(luaHeadTrackingStartChannel);
    LUA_FIELD_SHOW(luaBackpackTelemetry);
    LUA_FIELD_SHOW(luaBackpackVersion);
  }
}

static void setBleJoystickMode()
{
  setConnectionState(bleJoystick);
}

static void luahandWifiBle(struct luaPropertiesCommon *item, uint8_t arg)
{
  struct luaItem_command *cmd = (struct luaItem_command *)item;
  void (*setTargetState)();
  connectionState_e targetState;
  const char *textConfirm;
  const char *textRunning;
  if ((void *)item == (void *)&luaWebUpdate)
  {
    setTargetState = &setWifiUpdateMode;
    textConfirm = "Enter WiFi Update?";
    textRunning = "WiFi Running...";
    targetState = wifiUpdate;
  }
  else
  {
    setTargetState = &setBleJoystickMode;
    textConfirm = "Start BLE Joystick?";
    textRunning = "Joystick Running...";
    targetState = bleJoystick;
  }

  switch ((luaCmdStep_e)arg)
  {
    case lcsClick:
      if (connectionState == connected)
      {
        crsfTransmitter.sendLuaCommandResponse(cmd, lcsAskConfirm, textConfirm);
        return;
      }
      // fallthrough (clicking while not connected goes right to exectute)

    case lcsConfirmed:
      crsfTransmitter.sendLuaCommandResponse(cmd, lcsExecuting, textRunning);
      setTargetState();
      break;

    case lcsCancel:
      crsfTransmitter.sendLuaCommandResponse(cmd, lcsIdle, STR_EMPTYSPACE);
      if (connectionState == targetState)
      {
        rebootTime = millis() + 400;
      }
      break;

    default: // LUACMDSTEP_NONE on load, LUACMDSTEP_EXECUTING (our lua) or LUACMDSTEP_QUERY (Crossfire Config)
      crsfTransmitter.sendLuaCommandResponse(cmd, cmd->step, cmd->info);
      break;
  }
}

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
      EnterBindingModeSafely();
    }
    else if ((void *)item == (void *)&luaVtxSend)
    {
      VtxTriggerSend();
    }
    else if ((void *)item == (void *)&luaRxWebUpdate)
    {
      RxWiFiReadyToSend = true;
    }
    else if ((void *)item == (void *)&luaTxBackpackUpdate && OPT_USE_TX_BACKPACK)
    {
      TxBackpackWiFiReadyToSend = true;
    }
    else if ((void *)item == (void *)&luaVRxBackpackUpdate && OPT_USE_TX_BACKPACK)
    {
      VRxBackpackWiFiReadyToSend = true;
    }
    crsfTransmitter.sendLuaCommandResponse((struct luaItem_command *)item, lcsExecuting, msg);
  } /* if doExecute */
  else if(arg == lcsCancel || ((millis() - lastLcsPoll)> 2000))
  {
    crsfTransmitter.sendLuaCommandResponse((struct luaItem_command *)item, lcsIdle, STR_EMPTYSPACE);
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
    vtxFolderDynamicName[vtxFolderLabelOffset++] = '1' + config.GetVtxChannel();

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
 * @brief: Update the dynamic strings used for folder names and labels
 ***/
void TXModuleEndpoint::luadevUpdateFolderNames()
{
  updateFolderName_TxPower();
  updateFolderName_VtxAdmin();

  // These aren't folder names, just string labels slapped in the units field generally
  luadevUpdateTlmBandwidth();
  luadevUpdateBackpackOpts();
}

static void recalculatePacketRateOptions(int minInterval)
{
    const char *allRates = STR_LUA_PACKETRATES;
    const char *pos = allRates;
    luastrPacketRates[0] = 0;
    for (int i=0 ; i < RATE_MAX ; i++)
    {
        uint8_t rate = i;
        rate = RATE_MAX - 1 - rate;
        bool rateAllowed = (get_elrs_airRateConfig(rate)->interval * get_elrs_airRateConfig(rate)->numOfSends) >= minInterval;

#if defined(RADIO_LR1121)
        // Skip unsupported modes for hardware with only a single LR1121 or with a single RF path
        rateAllowed &= isSupportedRFRate(rate);
        if (rateAllowed)
        {
            const auto radio_type = get_elrs_airRateConfig(rate)->radio_type;
            if (rfMode == RF_MODE_900)
            {
                rateAllowed = radio_type == RADIO_TYPE_LR1121_GFSK_900 || radio_type == RADIO_TYPE_LR1121_LORA_900;
            }
            if (rfMode == RF_MODE_2G4)
            {
                rateAllowed = radio_type == RADIO_TYPE_LR1121_GFSK_2G4 || radio_type == RADIO_TYPE_LR1121_LORA_2G4;
            }
            if (rfMode == RF_MODE_DUAL)
            {
                rateAllowed = radio_type == RADIO_TYPE_LR1121_LORA_DUAL;
            }
        }
#endif
        const char *semi = strchrnul(pos, ';');
        if (rateAllowed)
        {
            strncat(luastrPacketRates, pos, semi - pos);
        }
        pos = semi;
        if (*semi == ';')
        {
            strcat(luastrPacketRates, ";");
            pos = semi+1;
        }
    }

    // trim off trailing semicolons (assumes luastrPacketRates has at least 1 non-semicolon)
    for (auto lastPos = strlen(luastrPacketRates)-1; luastrPacketRates[lastPos] == ';'; lastPos--)
    {
        luastrPacketRates[lastPos] = '\0';
    }
}

void TXModuleEndpoint::registerParameters()
{
    setLuaStringValue(&luaInfo, luaBadGoodString);
    luaRegisterDevicePingCallback(&pingCallback);

    if (HAS_RADIO) {
#if defined(RADIO_LR1121)
    // Copy the frequency part out of the domain to the display string
    char *bands = luastrRFBands;
    for (const char *domain = FHSSconfig->domain; *domain ; domain++)
    {
      if (isdigit(*domain))
      {
        *bands++ = *domain;
      }
    }
    *bands = '\0';
    strlcat(luastrRFBands, "MHz;2.4GHz", sizeof(luastrRFBands));
    // Only double LR1121 supports Dual Band modes
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
      strlcat(luastrRFBands, ";X-Band", sizeof(luastrRFBands));
    }

    registerLUAParameter(&luaRFBand, [](struct luaPropertiesCommon *item, uint8_t arg) {
      if (arg != rfMode)
      {
        // Choose the fastest supported packet rate in this RF band.
        rfMode = static_cast<RFMode>(arg);
        for (int i=0; i < RATE_MAX ; i++)
        {
          if (isSupportedRFRate(i))
          {
            const auto radio_type = get_elrs_airRateConfig(i)->radio_type;
            if (rfMode == RF_MODE_900 && (radio_type == RADIO_TYPE_LR1121_GFSK_900 || radio_type == RADIO_TYPE_LR1121_LORA_900))
            {
              config.SetRate(i);
              break;
            }
            if (rfMode == RF_MODE_2G4 && (radio_type == RADIO_TYPE_LR1121_GFSK_2G4 || radio_type == RADIO_TYPE_LR1121_LORA_2G4))
            {
              config.SetRate(i);
              break;
            }
            if (rfMode == RF_MODE_DUAL && radio_type == RADIO_TYPE_LR1121_LORA_DUAL)
            {
              config.SetRate(i);
              break;
            }
          }
        }
        recalculatePacketRateOptions(handset->getMinPacketInterval());
      }
    });
#endif
    registerLUAParameter(&luaAirRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
      if (arg < RATE_MAX)
      {
        uint8_t selectedRate = RATE_MAX - 1 - arg;
        uint8_t actualRate = adjustPacketRateForBaud(selectedRate);
        uint8_t newSwitchMode = adjustSwitchModeForAirRate(
          (OtaSwitchMode_e)config.GetSwitchMode(), get_elrs_airRateConfig(actualRate)->PayloadLength);
        // If the switch mode is going to change, block the change while connected
        bool isDisconnected = connectionState == disconnected;
        // Don't allow the switch mode to change if the TX is in mavlink mode
        // Wide switch mode is not compatible with mavlink, and the switch mode is
        // autoconfigured when entering mavlink mode
        bool isMavlinkMode = config.GetLinkMode() == TX_MAVLINK_MODE;
        if (newSwitchMode == OtaSwitchModeCurrent || (isDisconnected && !isMavlinkMode))
        {
          config.SetRate(actualRate);
          config.SetSwitchMode(newSwitchMode);
          if (actualRate != selectedRate)
          {
            setLuaWarningFlag(LUA_FLAG_ERROR_BAUDRATE, true);
          }
        }
        else
        {
          setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
        }
      }
    });
    registerLUAParameter(&luaTlmRate, [](struct luaPropertiesCommon *item, uint8_t arg) {
      expresslrs_tlm_ratio_e eRatio = (expresslrs_tlm_ratio_e)arg;
      if (eRatio <= TLM_RATIO_DISARMED)
      {
        bool isMavlinkMode = config.GetLinkMode() == TX_MAVLINK_MODE;
        // Don't allow TLM ratio changes if using AIRPORT or Mavlink
        if (!firmwareOptions.is_airport && !isMavlinkMode)
        {
          config.SetTlm(eRatio);
        }
      }
    });
    if (!firmwareOptions.is_airport)
    {
      registerLUAParameter(&luaSwitch, [](struct luaPropertiesCommon *item, uint8_t arg) {
        // Only allow changing switch mode when disconnected since we need to guarantee
        // the pack and unpack functions are matched
        bool isDisconnected = connectionState == disconnected;
        // Don't allow the switch mode to change if the TX is in mavlink mode
        // Wide switchmode is not compatible with mavlink, and the switchmode is
        // auto configuredwhen entering mavlink mode
        bool isMavlinkMode = config.GetLinkMode() == TX_MAVLINK_MODE;
        if (isDisconnected && !isMavlinkMode)
        {
          config.SetSwitchMode(arg);
          OtaUpdateSerializers((OtaSwitchMode_e)arg, ExpressLRS_currAirRate_Modparams->PayloadLength);
        }
        else if (!isMavlinkMode) // No need to display warning as no switch change can be made while in Mavlink mode.
        {
          setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
        }
      });
    }
    if (isDualRadio())
    {
      registerLUAParameter(&luaAntenna, [](struct luaPropertiesCommon *item, uint8_t arg) {
        // Force Gemini when using dual band modes.
        uint8_t newAntennaMode = get_elrs_airRateConfig(config.GetRate())->radio_type == RADIO_TYPE_LR1121_LORA_DUAL ? TX_RADIO_MODE_GEMINI : arg;
        config.SetAntennaMode(newAntennaMode);
      });
    }
    registerLUAParameter(&luaLinkMode, [](struct luaPropertiesCommon *item, uint8_t arg) {
      // Only allow changing when disconnected since we need to guarantee
      // the switch pack and unpack functions are matched on the tx and rx.
      bool isDisconnected = connectionState == disconnected;
      if (isDisconnected)
      {
        config.SetLinkMode(arg);
      }
      else
      {
        setLuaWarningFlag(LUA_FLAG_ERROR_CONNECTED, true);
      }
    });
    if (!firmwareOptions.is_airport)
    {
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
          msp.addByte(newModelMatch ? crsfTransmitter.modelId : 0xff);
          crsfRouter.AddMspMessage(&msp, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_CRSF_TRANSMITTER);
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
      config.SetVtxChannel(arg - 1);
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
  registerLUAParameter(&luaWiFiFolder);
  registerLUAParameter(&luaWebUpdate, &luahandWifiBle, luaWiFiFolder.common.id);
  if (HAS_RADIO) {
    registerLUAParameter(&luaRxWebUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);

    if (OPT_USE_TX_BACKPACK) {
      registerLUAParameter(&luaTxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
      registerLUAParameter(&luaVRxBackpackUpdate, &luahandSimpleSendCmd, luaWiFiFolder.common.id);
      // Backpack folder
      registerLUAParameter(&luaBackpackFolder);
      if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
      {
        registerLUAParameter(
            &luaBackpackEnable, [](luaPropertiesCommon *item, uint8_t arg) {
                // option is Off/On (enable) and config storage is On/Off (disable)
                config.SetBackpackDisable(arg == 0);
            }, luaBackpackFolder.common.id);
      }
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
      registerLUAParameter(
          &luaHeadTrackingEnableChannel, [](luaPropertiesCommon *item, uint8_t arg) {
              config.SetPTREnableChannel(arg);
          },
          luaBackpackFolder.common.id);
      registerLUAParameter(
          &luaHeadTrackingStartChannel, [](luaPropertiesCommon *item, uint8_t arg) {
              config.SetPTRStartChannel(arg);
          },
          luaBackpackFolder.common.id);
      registerLUAParameter(
            &luaBackpackTelemetry, [](luaPropertiesCommon *item, uint8_t arg) {
                config.SetBackpackTlmMode(arg);
                BackpackTelemReadyToSend = true;
            }, luaBackpackFolder.common.id);

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
}

void TXModuleEndpoint::updateParameters()
{
  bool isMavlinkMode = config.GetLinkMode() == TX_MAVLINK_MODE;
  uint8_t currentRate = adjustPacketRateForBaud(config.GetRate());
#if defined(RADIO_LR1121)
  // calculate RFMode from current packet-rate
  switch (get_elrs_airRateConfig(currentRate)->radio_type)
  {
    case RADIO_TYPE_LR1121_LORA_900:
    case RADIO_TYPE_LR1121_GFSK_900:
      rfMode = RF_MODE_900;
      break;
    case RADIO_TYPE_LR1121_LORA_DUAL:
      rfMode = RF_MODE_DUAL;
      break;
    default:
      rfMode = RF_MODE_2G4;
      break;
  }
  setLuaTextSelectionValue(&luaRFBand, rfMode);
#endif
  recalculatePacketRateOptions(handset->getMinPacketInterval());
  setLuaTextSelectionValue(&luaAirRate, RATE_MAX - 1 - currentRate);

  setLuaTextSelectionValue(&luaTlmRate, config.GetTlm());
  luaTlmRate.options = isMavlinkMode ? tlmRatiosMav : tlmRatios;

  luaAntenna.options = get_elrs_airRateConfig(config.GetRate())->radio_type == RADIO_TYPE_LR1121_LORA_DUAL ? antennamodeOptsDualBand : antennamodeOpts;

  setLuaTextSelectionValue(&luaSwitch, config.GetSwitchMode());
  if (isMavlinkMode)
  {
    luaSwitch.options = OtaIsFullRes ? switchmodeOpts8chMav : switchmodeOpts4chMav;
  }
  else
  {
    luaSwitch.options = OtaIsFullRes ? switchmodeOpts8ch : switchmodeOpts4ch;
  }

  if (isDualRadio())
  {
    setLuaTextSelectionValue(&luaAntenna, config.GetAntennaMode());
  }
  setLuaTextSelectionValue(&luaLinkMode, config.GetLinkMode());
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
  setLuaUint8Value(&luaVtxChannel, config.GetVtxChannel() + 1);
  setLuaTextSelectionValue(&luaVtxPwr, config.GetVtxPower());
  // Pit mode can only be sent as part of the power byte
  LUA_FIELD_VISIBLE(luaVtxPit, config.GetVtxPower() != 0);
  setLuaTextSelectionValue(&luaVtxPit, config.GetVtxPitmode());
  if (OPT_USE_TX_BACKPACK)
  {
    setLuaTextSelectionValue(&luaBackpackEnable, config.GetBackpackDisable() ? 0 : 1);
    setLuaTextSelectionValue(&luaDvrAux, config.GetBackpackDisable() ? 0 : config.GetDvrAux());
    setLuaTextSelectionValue(&luaDvrStartDelay, config.GetBackpackDisable() ? 0 : config.GetDvrStartDelay());
    setLuaTextSelectionValue(&luaDvrStopDelay, config.GetBackpackDisable() ? 0 : config.GetDvrStopDelay());
    setLuaTextSelectionValue(&luaHeadTrackingEnableChannel, config.GetBackpackDisable() ? 0 : config.GetPTREnableChannel());
    setLuaTextSelectionValue(&luaHeadTrackingStartChannel, config.GetBackpackDisable() ? 0 : config.GetPTRStartChannel());
    setLuaTextSelectionValue(&luaBackpackTelemetry, config.GetBackpackDisable() ? 0 : config.GetBackpackTlmMode());
    setLuaStringValue(&luaBackpackVersion, backpackVersion);
  }
  luadevUpdateFolderNames();
}
