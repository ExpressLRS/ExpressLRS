#ifdef TARGET_RX

#include "rxtx_devLua.h"

extern void deferExecution(uint32_t ms, std::function<void()> f);

extern bool InLoanBindingMode;
extern bool returnModelFromLoan;

static char modelString[] = "000";

#if defined(POWER_OUTPUT_VALUES)
static struct luaItem_selection luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};
#endif

static struct luaItem_selection luaRateInitIdx = {
    {"Init Rate", CRSF_TEXT_SELECTION},
    0, // value
    STR_LUA_PACKETRATES,
    STR_EMPTYSPACE
};

#if defined(GPIO_PIN_ANTENNA_SELECT)
static struct luaItem_selection luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna B;Antenna A;Diversity",
    STR_EMPTYSPACE
};
#endif

//----------------------------Info-----------------------------------

static struct luaItem_string luaModelNumber = {
    {"Model Id", CRSF_INFO},
    modelString
};

static struct luaItem_string luaELRSversion = {
    {version, CRSF_INFO},
    commit
};

//----------------------------Info-----------------------------------

//---------------------------- WiFi -----------------------------


//---------------------------- WiFi -----------------------------

//---------------------------- Model Loan Out -----------------------------

static struct luaItem_command luaLoanModel = {
    {"Loan Model", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static struct luaItem_command luaReturnModel = {
    {"Return Model", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

//---------------------------- Model Loan Out -----------------------------


static void registerLuaParameters()
{

  if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN)
  {
    registerLUAParameter(&luaAntennaMode, [](struct luaPropertiesCommon* item, uint8_t arg){
      config.SetAntennaMode(arg);
    });
  }
#if defined(POWER_OUTPUT_VALUES)
  luadevGeneratePowerOpts(&luaTlmPower);
  registerLUAParameter(&luaTlmPower, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetPower(arg);
    POWERMGNT::setPower((PowerLevels_e)constrain(arg + MinPower, MinPower, MaxPower));
  });
#endif
  registerLUAParameter(&luaRateInitIdx, [](struct luaPropertiesCommon* item, uint8_t arg) {
    uint8_t newRate = RATE_MAX - 1 - arg;
    config.SetRateInitialIdx(newRate);
  });
  registerLUAParameter(&luaLoanModel, [](struct luaPropertiesCommon* item, uint8_t arg){
    // Do it when polling for status i.e. going back to idle, because we're going to lose conenction to the TX
    if (arg == 6) {
      deferExecution(200, [](){ InLoanBindingMode = true; });
    }
    sendLuaCommandResponse(&luaLoanModel, arg < 5 ? lcsExecuting : lcsIdle, arg < 5 ? "Sending..." : "");
  });
  registerLUAParameter(&luaReturnModel, [](struct luaPropertiesCommon* item, uint8_t arg){
    // Do it when polling for status i.e. going back to idle, because we're going to lose conenction to the TX
    if (arg == 6) {
      deferExecution(200, []() { returnModelFromLoan = true; });
    }
    sendLuaCommandResponse(&luaReturnModel, arg < 5 ? lcsExecuting : lcsIdle, arg < 5 ? "Sending..." : "");
  });

  registerLUAParameter(&luaModelNumber);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

static int event()
{

  if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN)
  {
    setLuaTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
  }

#if defined(POWER_OUTPUT_VALUES)
  setLuaTextSelectionValue(&luaTlmPower, config.GetPower());
#endif
  setLuaTextSelectionValue(&luaRateInitIdx, RATE_MAX - 1 - config.GetRateInitialIdx());

  if (config.GetModelId() == 255)
  {
    setLuaStringValue(&luaModelNumber, "Off");
  }
  else
  {
    itoa(config.GetModelId(), modelString, 10);
    setLuaStringValue(&luaModelNumber, modelString);
  }
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
