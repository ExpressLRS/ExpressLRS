#include "../../src/targets.h"
#include "PFD.h"


//This impliments a phase frequnecy detector using a finite state machine

int32_t ICACHE_RAM_ATTR PFD::get_result()
{
  uint32_t diff = (timeSamples[1] - timeSamples[0]);
  return (int32_t)((uint32_t)up_state * diff) - ((uint32_t)down_state * diff);
}

void ICACHE_RAM_ATTR PFD::set_up_down_states()
{
  switch (PFD_state)
  {
  case 0:
    //Serial.println("case 0");
    up_state = 0;
    down_state = 1;
    break;

  case 1:
    //Serial.println("case 1");
    up_state = 0;
    down_state = 0;
    break;

  case 2:
    //Serial.println("case 2");
    up_state = 1;
    down_state = 0;
    break;
  }
}

void ICACHE_RAM_ATTR PFD::ref_rising()
{
  uint32_t now = micros();
  if (PFD_state != 2)
  {
    PFD_state++;
  }
  if (timeSamplesCounter <= 1)
  {
    timeSamples[timeSamplesCounter] = now;
    timeSamplesCounter++;
  }
}

void ICACHE_RAM_ATTR PFD::nco_rising()
{
  uint32_t now = micros();
  if (PFD_state != 0)
  {
    PFD_state--;
  }
  if (timeSamplesCounter <= 1)
  {
    timeSamples[timeSamplesCounter] = now;
    timeSamplesCounter++;
  }
}

void ICACHE_RAM_ATTR PFD::reset()
{
  PFD_state = 0;
  timeSamples[0] = 0;
  timeSamples[1] = 0;
  timeSamplesCounter = 0;
  ref_state = 0;
  nco_state = 0;
}
