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
#include "FHSS.h"

extern bool InLoanBindingMode;
extern bool returnModelFromLoan;

static const char emptySpace[1] = {0};
static char modelString[] = "000";

#ifdef POWER_OUTPUT_VALUES
static char strPowerLevels[] = "10;25;50;100;250;500;1000;2000";
#endif


#ifdef POWER_OUTPUT_VALUES
static struct luaItem_selection luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};
#endif

#if defined(GPIO_PIN_ANTENNA_SELECT)
static struct luaItem_selection luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna B;Antenna A;Diversity",
    emptySpace
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
    emptySpace
};

static struct luaItem_command luaReturnModel = {
    {"Return Model", CRSF_COMMAND},
    lcsIdle, // step
    emptySpace
};

//---------------------------- Model Loan Out -----------------------------


extern RxConfig config;

#ifdef POWER_OUTPUT_VALUES
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
  luaTlmPower.options = (const char *)out;

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
#endif

static void registerLuaParameters()
{

  if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN)
  {
    registerLUAParameter(&luaAntennaMode, [](struct luaPropertiesCommon* item, uint8_t arg){
      config.SetAntennaMode(arg);
    });
  }
#ifdef POWER_OUTPUT_VALUES
  luadevGeneratePowerOpts();
  registerLUAParameter(&luaTlmPower, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetPower(arg);
    POWERMGNT::setPower((PowerLevels_e)constrain(arg + MinPower, MinPower, MaxPower));
  });
#endif
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

#if defined(GPIO_PIN_ANTENNA_SELECT)
  setLuaTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
#endif

#ifdef POWER_OUTPUT_VALUES
  setLuaTextSelectionValue(&luaTlmPower, config.GetPower());
#endif

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
