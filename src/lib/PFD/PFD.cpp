#include "../../src/targets.h"
#include "PFD.h"

// measures times differences to calculate phase error 

void ICACHE_RAM_ATTR PFD::calc_result()
{
  if (got_ref && got_nco)
  {
    result = (int32_t)(timeSamples_ref - timeSamples_nco);
  }
  else
  {
    result = 0;
  }
}

int32_t ICACHE_RAM_ATTR PFD::get_result()
{
  return result;
}

void ICACHE_RAM_ATTR PFD::ref_rising(uint32_t time)
{
  timeSamples_ref = time;
  got_ref = true;
}

void ICACHE_RAM_ATTR PFD::nco_rising(uint32_t time)
{
  timeSamples_nco = time;
  got_nco = true;
}

void ICACHE_RAM_ATTR PFD::reset()
{
  got_ref = false;
  got_nco = false;
}
