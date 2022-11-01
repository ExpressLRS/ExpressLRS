#include "rxtx_devLua.h"
#include "POWERMGNT.h"

char strPowerLevels[] = "10;25;50;100;250;500;1000;2000";
const char STR_EMPTYSPACE[] = { 0 };
const char STR_LUA_PACKETRATES[] =
#if defined(RADIO_SX127X)
    "L25(-123dBm);L50(-120dBm);L100(-117dBm);L100F(-112dBm);L200(-112dBm)";
#elif defined(RADIO_SX128X)
    "L50(-115dBm);L100F(-112dBm);L150(-112dBm);L250(-108dBm);L333F(-105dBm);L500(-105dBm);"
    "D250(-104dBm);D500(-104dBm);F500(-104dBm);F1000(-104dBm)";
#else
    #error Invalid radio configuration!
#endif

void luadevGeneratePowerOpts(luaItem_selection *luaPower)
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
  // luaPower->min = (uint8_t)MinPower;
  luaPower->options = (const char *)out;

  // Continue until after than MAXth item and drop a null in the orginal
  // string on the semicolon (not after like the previous loop)
  while (pwr <= POWERMGNT::getMaxPower())
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
