/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * This file provides utilities for packing and unpacking the data to
 * be sent over the radio link.
 */

#include "OTA.h"
#include "channels.h"

OTA ota;

static uint8_t sentSwitches[N_SWITCHES] = {0};

/**
 * Convert the rc data corresponding to switches to 3 bit values.
 * The output is mapped evenly across 6 output values (0-5)
 * With a special value 7 indicating the middle so it works
 * with switches with a middle position as well as 6-position
 */
void ICACHE_RAM_ATTR OTA::updateSwitchValues(Channels* chan)
{
  // AUX1 is arm switch, one bit
  CurrentSwitches[0] = CRSF_to_BIT(chan->ChannelData[4]);

  // AUX2-(N-1) are Low Resolution, "7pos" (6+center)
  const uint16_t CHANNEL_BIN_COUNT = 6;
  const uint16_t CHANNEL_BIN_SIZE = CRSF_CHANNEL_VALUE_SPAN / CHANNEL_BIN_COUNT;
  for (int i = 1; i < N_SWITCHES - 1; i++) {
    uint16_t ch = chan->ChannelData[i + 4];
    // If channel is within 1/4 a BIN of being in the middle use special value 7
    if (ch < (CRSF_CHANNEL_VALUE_MID - CHANNEL_BIN_SIZE / 4) ||
        ch > (CRSF_CHANNEL_VALUE_MID + CHANNEL_BIN_SIZE / 4))
      CurrentSwitches[i] = CRSF_to_N(ch, CHANNEL_BIN_COUNT);
    else
      CurrentSwitches[i] = 7;
  }  // for N_SWITCHES

  // AUXx is High Resolution 16-pos (4-bit)
  CurrentSwitches[N_SWITCHES - 1] = CRSF_to_N(chan->ChannelData[N_SWITCHES - 1 + 4], 16);
}

#ifdef UNIT_TEST
/**
 * Record the value of a switch that was sent to the rx
 */
void OTA::setSentSwitch(uint8_t index, uint8_t value)
{
    sentSwitches[index] = value;
}

void OTA::setCurrentSwitch(uint8_t index, uint8_t value)
{
    CurrentSwitches[index] = value;
}
#endif

uint8_t ICACHE_RAM_ATTR OTA::getNextSwitchIndex()
{
    int firstSwitch = 0; // sequential switches includes switch 0

#if defined HYBRID_SWITCHES_8
    firstSwitch = 1; // skip 0 since it is sent on every packet
#endif

    // look for a changed switch
    int i;
    for (i = firstSwitch; i < N_SWITCHES; i++)
    {
        if (CurrentSwitches[i] != sentSwitches[i]) //
            break;
    }
    // if we didn't find a changed switch, we get here with i==N_SWITCHES
    if (i == N_SWITCHES)
    {
        i = NextSwitchIndex;
    }

    // keep track of which switch to send next if there are no changed switches
    // during the next call.
    NextSwitchIndex = (i + 1) % 8;

#ifdef HYBRID_SWITCHES_8
    // for hydrid switches 0 is sent on every packet, skip it in round-robin
    if (NextSwitchIndex == 0)
    {
        NextSwitchIndex = 1;
    }
#endif

    return i;
}

/**
 * Hybrid switches packet encoding for sending over the air
 *
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 2 bit value is used to send the remaining switches
 * in a round-robin fashion.
 * If any of the round-robin switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 *
 * Inputs: channels, currentSwitches
 * Outputs: Radio.TXdataBuffer, side-effects the sentSwitch value
 */
#ifdef ENABLE_TELEMETRY
void ICACHE_RAM_ATTR OTA::GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, Channels* chan, bool TelemetryStatus)
#else
void ICACHE_RAM_ATTR OTA::GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, Channels* chan)
#endif
{
  volatile uint16_t* channels = chan->ChannelData;

  Buffer[0] = RC_DATA_PACKET & 0b11;
  Buffer[1] = ((channels[0]) >> 3);
  Buffer[2] = ((channels[1]) >> 3);
  Buffer[3] = ((channels[2]) >> 3);
  Buffer[4] = ((channels[3]) >> 3);
  Buffer[5] = ((channels[0] & 0b110) << 5) |
              ((channels[1] & 0b110) << 3) |
              ((channels[2] & 0b110) << 1) |
              ((channels[3] & 0b110) >> 1);

  // find the next switch to send
  uint8_t nextSwitchIndex = ota.getNextSwitchIndex(); // needs to go away
  // Actually send switchIndex - 1 in the packet, to shift down 1-7 (0b111) to
  // 0-6 (0b110) If the two high bits are 0b11, the receiver knows it is the
  // last switch and can use that bit to store data
  uint8_t bitclearedSwitchIndex = nextSwitchIndex - 1;
  // currentSwitches[] is 0-15 for index 1, 0-2 for index 2-7
  // Rely on currentSwitches to *only* have values in that rang
  uint8_t value = ota.CurrentSwitches[nextSwitchIndex];

  Buffer[6] =
#ifdef ENABLE_TELEMETRY
      TelemetryStatus << 7 |
#endif
      // switch 0 is one bit sent on every packet - intended for low latency
      // arm/disarm
      ota.CurrentSwitches[0] << 6 |
      // tell the receiver which switch index this is
      bitclearedSwitchIndex << 3 |
      // include the switch value
      value;

  // update the sent value
  sentSwitches[nextSwitchIndex] = value;
}

/**
 * Hybrid switches decoding of over the air data
 *
 * Hybrid switches uses 10 bits for each analog channel,
 * 2 bits for the low latency switch[0]
 * 3 bits for the round-robin switch index and 2 bits for the value
 *
 * Input: Buffer
 * Output: crsf->PackedRCdataOut
 */
void ICACHE_RAM_ATTR OTA::UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, Channels* chan)
{
    // The analog channels
    chan->PackedRCdataOut.ch0 = (Buffer[1] << 3) | ((Buffer[5] & 0b11000000) >> 5);
    chan->PackedRCdataOut.ch1 = (Buffer[2] << 3) | ((Buffer[5] & 0b00110000) >> 3);
    chan->PackedRCdataOut.ch2 = (Buffer[3] << 3) | ((Buffer[5] & 0b00001100) >> 1);
    chan->PackedRCdataOut.ch3 = (Buffer[4] << 3) | ((Buffer[5] & 0b00000011) << 1);

    // The low latency switch
    chan->PackedRCdataOut.ch4 = BIT_to_CRSF((Buffer[6] & 0b01000000) >> 6);

    // The round-robin switch, switchIndex is actually index-1 
    // to leave the low bit open for switch 7 (sent as 0b11x)
    // where x is the high bit of switch 7
    uint8_t switchIndex = (Buffer[6] & 0b111000) >> 3;
    uint16_t switchValue = SWITCH3b_to_CRSF(Buffer[6] & 0b111);

    switch (switchIndex) {
        case 0:  
            chan->PackedRCdataOut.ch5 = switchValue;
            break;
        case 1:
            chan->PackedRCdataOut.ch6 = switchValue;
            break;
        case 2:
            chan->PackedRCdataOut.ch7 = switchValue;
            break;
        case 3:
            chan->PackedRCdataOut.ch8 = switchValue;
            break;
        case 4:
            chan->PackedRCdataOut.ch9 = switchValue;
            break;
        case 5:
            chan->PackedRCdataOut.ch10 = switchValue;
            break;
        case 6:   // Because AUX1 (index 0) is the low latency switch, the low bit
        case 7:   // of the switchIndex can be used as data, and arrives as index "6"
            chan->PackedRCdataOut.ch11 = N_to_CRSF(Buffer[6] & 0b1111, 15);
            break;
    }
}




#ifdef ENABLE_TELEMETRY
void ICACHE_RAM_ATTR OTA::GenerateChannelData10bit(volatile uint8_t* Buffer,
                                              Channels* chan, bool TelemetryStatus)
#else
void ICACHE_RAM_ATTR OTA::GenerateChannelData10bit(volatile uint8_t* Buffer, Channels* chan)
#endif
{
  if (!chan) return;
  volatile uint16_t* channels = chan->ChannelData;

  Buffer[0] = RC_DATA_PACKET & 0b11;
  Buffer[1] = ((channels[0]) >> 3);
  Buffer[2] = ((channels[1]) >> 3);
  Buffer[3] = ((channels[2]) >> 3);
  Buffer[4] = ((channels[3]) >> 3);
  Buffer[5] = ((channels[0] & 0b110) << 5) |
                           ((channels[1] & 0b110) << 3) |
                           ((channels[2] & 0b110) << 1) |
                           ((channels[3] & 0b110) >> 1);
  Buffer[6] = CRSF_to_BIT(channels[4]) << 7;
  Buffer[6] |= CRSF_to_BIT(channels[5]) << 6;
  Buffer[6] |= CRSF_to_BIT(channels[6]) << 5;
  Buffer[6] |= CRSF_to_BIT(channels[7]) << 4;
  Buffer[6] |= CRSF_to_BIT(channels[8]) << 3;
  Buffer[6] |= CRSF_to_BIT(channels[9]) << 2;
  Buffer[6] |= CRSF_to_BIT(channels[10]) << 1;
  Buffer[6] |= CRSF_to_BIT(channels[11]) << 0;
}


void ICACHE_RAM_ATTR OTA::UnpackChannelData10bit(volatile uint8_t* Buffer, Channels *chan)
{
    chan->PackedRCdataOut.ch0 = (Buffer[1] << 3) | ((Buffer[5] & 0b11000000) >> 5);
    chan->PackedRCdataOut.ch1 = (Buffer[2] << 3) | ((Buffer[5] & 0b00110000) >> 3);
    chan->PackedRCdataOut.ch2 = (Buffer[3] << 3) | ((Buffer[5] & 0b00001100) >> 1);
    chan->PackedRCdataOut.ch3 = (Buffer[4] << 3) | ((Buffer[5] & 0b00000011) << 1);
    chan->PackedRCdataOut.ch4 = BIT_to_CRSF(Buffer[6] & 0b10000000);
    chan->PackedRCdataOut.ch5 = BIT_to_CRSF(Buffer[6] & 0b01000000);
    chan->PackedRCdataOut.ch6 = BIT_to_CRSF(Buffer[6] & 0b00100000);
    chan->PackedRCdataOut.ch7 = BIT_to_CRSF(Buffer[6] & 0b00010000);
    chan->PackedRCdataOut.ch8 = BIT_to_CRSF(Buffer[6] & 0b00001000);
    chan->PackedRCdataOut.ch9 = BIT_to_CRSF(Buffer[6] & 0b00000100);
    chan->PackedRCdataOut.ch10 = BIT_to_CRSF(Buffer[6] & 0b00000010);
    chan->PackedRCdataOut.ch11 = BIT_to_CRSF(Buffer[6] & 0b00000001);
}

void OTA::init(Mode m)
{
  if (OTA::HybridSwitches8 == m) {
    GenerateChannelData = GenerateChannelDataHybridSwitch8;
    UnpackChannelData = UnpackChannelDataHybridSwitch8;
  }
  else {
    GenerateChannelData = GenerateChannelData10bit;
    UnpackChannelData = UnpackChannelData10bit;
  }     
}

