#include "rxtx_devLua.h"
#include "POWERMGNT.h"

char strPowerLevels[] = "10;25;50;100;250;500;1000;2000;MatchTX ";

void luadevGeneratePowerOpts(selectionParameter *luaPower)
{
#ifndef UNIT_TEST
  // This function modifies the strPowerLevels in place and must not
  // be called more than once!
  char *out = strPowerLevels;
  PowerLevels_e pwr = PWR_10mW;
  // Count the semicolons to move `out` to point to the MINth item
  while (pwr < POWERMGNT::getMinPower())
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

#if defined(TARGET_RX)
  // The RX has the dynamic option added on to the end
  // the space on the end is to make it display "MatchTX mW"
  // but only if it has more than one power level
  if (POWERMGNT::getMinPower() != POWERMGNT::getMaxPower())
    strcat(strPowerLevels, ";MatchTX ");
#endif
#endif
}
