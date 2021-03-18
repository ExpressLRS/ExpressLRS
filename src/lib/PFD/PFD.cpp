#include "../../src/targets.h"
#include "PFD.h"

//This impliments a phase frequnecy detector using a finite state machine

void ICACHE_RAM_ATTR PFD::calc_result()
{
  result = (int32_t)(timeSamples_ref - timeSamples_nco);
}

int32_t ICACHE_RAM_ATTR PFD::get_result()
{
  return result;
}

void ICACHE_RAM_ATTR PFD::ref_rising(uint32_t time)
{
  timeSamples_ref = time;
}

void ICACHE_RAM_ATTR PFD::nco_rising(uint32_t time)
{
  timeSamples_nco = time;
}

void ICACHE_RAM_ATTR PFD::reset()
{
  timeSamples_nco = 0;
  timeSamples_ref = 0;
}
