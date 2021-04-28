#include "channels.h"
#include "crsf_protocol.h"

volatile uint16_t ChannelData[16] = {0};

uint8_t CurrentSwitches[N_SWITCHES];
uint8_t SentSwitches[N_SWITCHES];

// which switch should be sent in the next rc packet
uint8_t NextSwitchIndex;

/**
 * Convert the rc data corresponding to switches to 3 bit values.
 * The output is mapped evenly across 6 output values (0-5)
 * With a special value 7 indicating the middle so it works
 * with switches with a middle position as well as 6-position
 */
void ICACHE_RAM_ATTR updateSwitchValues(volatile uint16_t *channels)
{
  if (!channels) return;

  // AUX1 is arm switch, one bit
  CurrentSwitches[0] = CRSF_to_BIT(channels[4]);

  // AUX2-(N-1) are Low Resolution, "7pos" (6+center)
  const uint16_t CHANNEL_BIN_COUNT = 6;
  const uint16_t CHANNEL_BIN_SIZE = CRSF_CHANNEL_VALUE_SPAN / CHANNEL_BIN_COUNT;
  for (int i = 1; i < N_SWITCHES - 1; i++) {
    uint16_t ch = channels[i + 4];
    // If channel is within 1/4 a BIN of being in the middle use special value 7
    if (ch < (CRSF_CHANNEL_VALUE_MID - CHANNEL_BIN_SIZE / 4) ||
        ch > (CRSF_CHANNEL_VALUE_MID + CHANNEL_BIN_SIZE / 4))
      CurrentSwitches[i] = CRSF_to_N(ch, CHANNEL_BIN_COUNT);
    else
      CurrentSwitches[i] = 7;
  }  // for N_SWITCHES

  // AUXx is High Resolution 16-pos (4-bit)
  CurrentSwitches[N_SWITCHES - 1] = CRSF_to_N(channels[N_SWITCHES - 1 + 4], 16);
}
