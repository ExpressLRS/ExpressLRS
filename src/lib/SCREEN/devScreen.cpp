#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) || defined(HAS_TFT_SCREEN)

#include "logging.h"
#include "Wire.h"
#include "config.h"
#include "POWERMGNT.h"
#include "hwTimer.h"

#ifdef HAS_TFT_SCREEN
#include "TFT/tftscreen.h"
TFTScreen screen;
#else
#include "OLED/oledscreen.h"
OLEDScreen screen;
#endif

#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton/FiveWayButton.h"
FiveWayButton fivewaybutton;
static uint32_t last_user_input_ms;
#endif

#ifdef HAS_GSENSOR
#include "gsensor.h"
extern Gsensor gsensor;
static bool is_screen_flipped = false;
static bool is_pre_screen_flipped = false;
#endif

#ifdef HAS_THERMAL
#include "thermal.h"
extern Thermal thermal;

#define UPDATE_TEMP_TIMEOUT  5000
static uint32_t last_update_temp_ms;
#endif

#define SCREEN_DURATION 20

#define LOGO_DISPLAY_TIMEOUT  5000
static bool isLogoDisplayed = false;

#define SCREEN_IDLE_TIMEOUT  20000

extern bool connectionHasModelMatch;
extern bool ICACHE_RAM_ATTR IsArmed();
extern void EnterBindingMode();
extern void ExitBindingMode();

#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif

#ifdef PLATFORM_ESP32
extern unsigned long rebootTime;
#endif

#define BINDING_MODE_TIME_OUT 5000
static uint32_t binding_mode_start_ms = 0;


static void ScreenUpdateCallback(int updateType)
{
  switch(updateType)
  {
    case USER_UPDATE_TYPE_RATE:
      DBGLN("User set AirRate %d", screen.getUserRateIndex());
      config.SetRate(screen.getUserRateIndex());
      break;
    case USER_UPDATE_TYPE_POWER:
      DBGLN("User set Power %d", screen.getUserPowerIndex());
      config.SetPower(screen.getUserPowerIndex());
      break;
    case USER_UPDATE_TYPE_RATIO:
      DBGLN("User set TLM RATIO %d", screen.getUserRatioIndex());
      config.SetTlm(screen.getUserRatioIndex());
      break;
    case USER_UPDATE_TYPE_BINDING:
      DBGLN("User request binding!");
      EnterBindingMode();
      binding_mode_start_ms = millis();
      break;
    case USER_UPDATE_TYPE_EXIT_BINDING:
      DBGLN("User request exit binding!");
      ExitBindingMode();
      break;
    case USER_UPDATE_TYPE_WIFI:
      DBGLN("User request Wifi Update Mode!");
      connectionState = wifiUpdate;
      break;
    case USER_UPDATE_TYPE_EXIT_WIFI:
      DBGLN("User request exit Wifi Update Mode!");
#ifdef PLATFORM_ESP32
      if (connectionState == wifiUpdate) {
        rebootTime = millis() + 200;
      }
#endif
      break;
#ifdef HAS_THERMAL
    case USER_UPDATE_TYPE_SMARTFAN:
      DBGLN("User request SMART FAN Mode!");
      config.SetFanMode(screen.getUserSmartFanIndex());
      break;
#endif
#ifdef HAS_GSENSOR
    case USER_UPDATE_TYPE_POWERSAVING:
      DBGLN("User request Power Saving Mode!");
      config.SetMotionMode(screen.getUserPowerSavingIndex());
      break;
#endif
    default:
      DBGLN("Error handle user request %d", updateType);
      break;
  }
}

static void devScreenPushParamUpdate()
{
  uint8_t disp_message = IsArmed() ? SCREEN_MSG_ARMED : ((connectionState == connected) ? (connectionHasModelMatch ? SCREEN_MSG_CONNECTED : SCREEN_MSG_MISMATCH) : SCREEN_MSG_DISCONNECTED);
  screen.doParamUpdate(config.GetRate(), config.GetPower(), config.GetTlm(), config.GetMotionMode(), config.GetFanMode(), config.GetDynamicPower(), (uint8_t)(POWERMGNT::currPower()), disp_message);
}


#ifdef HAS_FIVE_WAY_BUTTON
static int handle(void)
{
#if defined(JOY_ADC_VALUES) && defined(PLATFORM_ESP32)
  // if we are using analog joystick then we can't cancel because WiFi is using the ADC2 (i.e. channel >= 8)!
  if (connectionState == wifiUpdate && digitalPinToAnalogChannel(GPIO_PIN_JOYSTICK) >= 8)
  {
    return DURATION_NEVER;
  }
#endif

#ifdef HAS_GSENSOR
  is_screen_flipped = gsensor.isFlipped();

  if ((is_screen_flipped == true) && (is_pre_screen_flipped == false))
  {
    screen.doScreenBackLight(SCREEN_BACKLIGHT_OFF);
  }
  else if ((is_screen_flipped == false) && (is_pre_screen_flipped == true))
  {
    screen.doScreenBackLight(SCREEN_BACKLIGHT_ON);
  }
  is_pre_screen_flipped = is_screen_flipped;
  if (is_screen_flipped)
  {
    return 100; // no need to check as often if the screen is off!
  }
#endif
  if(!IsArmed())
  {
    int key;
    bool isLongPressed;
    fivewaybutton.update(&key, &isLongPressed);

    uint32_t now = millis();
    if (key != INPUT_KEY_NO_PRESS)
    {
      last_user_input_ms = now;
    }

    if(screen.getScreenStatus() == SCREEN_STATUS_IDLE)
    {
#ifdef HAS_THERMAL
      if(now - last_update_temp_ms > UPDATE_TEMP_TIMEOUT)
      {
        screen.doTemperatureUpdate(thermal.getTempValue());
        last_update_temp_ms = now;
      }
#endif
      if(isLongPressed)
      {
        screen.activeScreen();
      }
    }
    else if(screen.getScreenStatus() == SCREEN_STATUS_WORK)
    {
      if (key != INPUT_KEY_NO_PRESS)
      {
        DBGLN("user key = %d", key);
        if(key == INPUT_KEY_DOWN_PRESS)
        {
          screen.doUserAction(USER_ACTION_DOWN);
        }
        else if(key == INPUT_KEY_UP_PRESS)
        {
          screen.doUserAction(USER_ACTION_UP);
        }
        else if(key == INPUT_KEY_LEFT_PRESS)
        {
          screen.doUserAction(USER_ACTION_LEFT);
        }
        else if(key == INPUT_KEY_RIGHT_PRESS)
        {
          screen.doUserAction(USER_ACTION_RIGHT);
        }
        else if(key == INPUT_KEY_OK_PRESS)
        {
          screen.doUserAction(USER_ACTION_CONFIRM);
        }
      }
      // timeout to the idle screen if not in wifi mode
      else if((now - last_user_input_ms) > SCREEN_IDLE_TIMEOUT && (connectionState != wifiUpdate))
      {
        screen.idleScreen();
      }
    }
    else if(screen.getScreenStatus() == SCREEN_STATUS_BINDING)
    {
      if((now - binding_mode_start_ms) > BINDING_MODE_TIME_OUT)
      {
        screen.doUserAction(USER_ACTION_LEFT);
      }
    }
  }
  else if(screen.getScreenStatus() != SCREEN_STATUS_IDLE)
  {
    screen.idleScreen();
  }
  return SCREEN_DURATION;
}
#else
static int handle(void)
{
  return DURATION_NEVER;
}
#endif

static void initialize()
{
  #ifdef HAS_FIVE_WAY_BUTTON
  fivewaybutton.init();
  #endif
  screen.updatecallback = &ScreenUpdateCallback;
  #if defined(PLATFORM_ESP32)
  screen.init(esp_reset_reason() == ESP_RST_SW);
  #else
  screen.init(false);
  #endif
}

static int start()
{
  if (screen.getScreenStatus() == SCREEN_STATUS_INIT)
  {
    devScreenPushParamUpdate();
    return LOGO_DISPLAY_TIMEOUT;
  }
  return DURATION_IMMEDIATELY;
}

static int event()
{
  if (connectionState == wifiUpdate)
  {
    screen.setInWifiMode();
  }
  else
  {
    devScreenPushParamUpdate();
  }

  return DURATION_IGNORE;
}

static int timeout()
{
  if(screen.getScreenStatus() == SCREEN_STATUS_INIT)
  {
    isLogoDisplayed = true;
    screen.idleScreen();
  }

  return handle();
}

device_t Screen_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};
#endif