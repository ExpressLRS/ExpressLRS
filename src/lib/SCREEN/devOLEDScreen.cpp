#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C)

#include "targets.h"
#include "common.h"
#include "device.h"


#include "logging.h"
#include "Wire.h"
#include "config.h"
#include "POWERMGNT.h"
#include "hwTimer.h"

#include "tftscreen.h"
TFTScreen screen;

#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton.h"
FiveWayButton fivewaybutton;
#endif

#define SCREEN_DURATION 20

#define LOGO_DISPLAY_TIMEOUT  5000
boolean isLogoDisplayed = false;

uint32_t none_input_start_time = 0;
boolean isUserInputCheck = false;
#define SCREEN_IDLE_TIMEOUT  20000

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
uint32_t binding_mode_start_time = 0;


void ScreenUpdateCallback(int updateType)
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
      binding_mode_start_time = millis();
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
      INFOLN("User request exit Wifi Update Mode!");
#ifdef PLATFORM_ESP32
      if (connectionState == wifiUpdate) {
        rebootTime = millis() + 200;
      }
#endif
      break;
    default:
      DBGLN("Error handle user request %d", updateType);
      break;
  }
}

#ifdef HAS_FIVE_WAY_BUTTON
void handle(void)
{
  fivewaybutton.handle();

  if(!IsArmed())
  {
    int key;
    boolean isLongPressed;
    fivewaybutton.getKeyState(&key, &isLongPressed);
    if(screen.getScreenStatus() == SCREEN_STATUS_IDLE)
    {   
      if(isLongPressed)
      {
        screen.activeScreen();
      }
    }
    else if(screen.getScreenStatus() == SCREEN_STATUS_WORK)
    {
      if(!isUserInputCheck)
      {
        none_input_start_time = millis();
        isUserInputCheck = true;
      }

      if(key != INPUT_KEY_NO_PRESS)
      {
        INFOLN("user key = %d", key);
        isUserInputCheck = false;
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
      else
      {
        if((millis() - none_input_start_time) > SCREEN_IDLE_TIMEOUT)
        {
          isUserInputCheck = false;
          screen.idleScreen();
        }
      }
    }
    else if(screen.getScreenStatus() == SCREEN_STATUS_BINDING)
    {
      if((millis() - binding_mode_start_time) > BINDING_MODE_TIME_OUT)
      {
        screen.doUserAction(USER_ACTION_LEFT);
      }
    }
  }
}
#endif

static void initialize()
{
//   Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);
  #ifdef HAS_FIVE_WAY_BUTTON
  fivewaybutton.init();
  #endif
  screen.updatecallback = &ScreenUpdateCallback;
  screen.init();
}

static int start()
{
    screen.doParamUpdate(config.GetRate(), (uint8_t)(POWERMGNT::currPower()), config.GetTlm(), config.GetMotionMode(), config.GetFanMode());
    return LOGO_DISPLAY_TIMEOUT;
}

static int event()
{
  if(!isLogoDisplayed)
  {
    return DURATION_IGNORE; // we are still displaying the startup message, so don't change the timeout
  }
  if(connectionState != wifiUpdate)
  {
      screen.doParamUpdate(config.GetRate(), (uint8_t)(POWERMGNT::currPower()), config.GetTlm(), config.GetMotionMode(), config.GetFanMode());
  }

  return SCREEN_DURATION;
}

static int timeout()
{
  if(screen.getScreenStatus() == SCREEN_STATUS_INIT)
  {
    isLogoDisplayed = true;
    screen.idleScreen();
  }

  handle();
  return SCREEN_DURATION;

}

device_t Screen_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};
#endif