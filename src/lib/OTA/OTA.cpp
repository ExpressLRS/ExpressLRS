/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * This file provides utilities for packing and unpacking the data to
 * be sent over the radio link.
 */

#include "OTA.h"

#if TARGET_TX or defined UNIT_TEST

/**
 * Hybrid switches packet encoding for sending over the air
 *
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 3-4 bit value is used to send the remaining switches
 * in a round-robin fashion.
 *
 * Inputs: crsf.ChannelDataIn, crsf.currentSwitches
 * Outputs: Radio.TXdataBuffer, side-effects the sentSwitch value
 */
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf, bool TelemetryStatus)
{
  Buffer[0] = RC_DATA_PACKET & 0b11;
  Buffer[1] = ((crsf->ChannelDataIn[0]) >> 3);
  Buffer[2] = ((crsf->ChannelDataIn[1]) >> 3);
  Buffer[3] = ((crsf->ChannelDataIn[2]) >> 3);
  Buffer[4] = ((crsf->ChannelDataIn[3]) >> 3);
  Buffer[5] = ((crsf->ChannelDataIn[0] & 0b110) << 5) |
                           ((crsf->ChannelDataIn[1] & 0b110) << 3) |
                           ((crsf->ChannelDataIn[2] & 0b110) << 1) |
                           ((crsf->ChannelDataIn[3] & 0b110) >> 1);

  // Actually send switchIndex - 1 in the packet, to shift down 1-7 (0b111) to 0-6 (0b110)
  // If the two high bits are 0b11, the receiver knows it is the last switch and can use
  // that bit to store data
  static uint8_t bitclearedSwitchIndex = 0;
  uint8_t value;
  // AUX8 is High Resolution 16-pos (4-bit)
  if (bitclearedSwitchIndex == 6)
    value = CRSF_to_N(crsf->ChannelDataIn[6 + 1 + 4], 16);
  else
  {
    // AUX2-7 are Low Resolution, "7pos" 6+center (3-bit)
    // The output is mapped evenly across 6 output values (0-5)
    // with a special value 7 indicating the middle so it works
    // with switches with a middle position as well as 6-position
    const uint16_t CHANNEL_BIN_COUNT = 6;
    const uint16_t CHANNEL_BIN_SIZE = CRSF_CHANNEL_VALUE_SPAN / CHANNEL_BIN_COUNT;
    uint16_t ch = crsf->ChannelDataIn[bitclearedSwitchIndex + 1 + 4];
    // If channel is within 1/4 a BIN of being in the middle use special value 7
    if (ch < (CRSF_CHANNEL_VALUE_MID-CHANNEL_BIN_SIZE/4)
        || ch > (CRSF_CHANNEL_VALUE_MID+CHANNEL_BIN_SIZE/4))
        value = CRSF_to_N(ch, CHANNEL_BIN_COUNT);
    else
        value = 7;
  } // If not 16-pos

  Buffer[6] =
      TelemetryStatus << 7 |
      // switch 0 is one bit sent on every packet - intended for low latency arm/disarm
      CRSF_to_BIT(crsf->ChannelDataIn[4]) << 6 |
      // tell the receiver which switch index this is
      bitclearedSwitchIndex << 3 |
      // include the switch value
      value;

  // update the sent value
  bitclearedSwitchIndex = (bitclearedSwitchIndex + 1) % 7;
}
#endif

#if TARGET_RX or defined UNIT_TEST

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
void ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf)
{
    // The analog channels
    crsf->PackedRCdataOut.ch0 = (Buffer[1] << 3) | ((Buffer[5] & 0b11000000) >> 5);
    crsf->PackedRCdataOut.ch1 = (Buffer[2] << 3) | ((Buffer[5] & 0b00110000) >> 3);
    crsf->PackedRCdataOut.ch2 = (Buffer[3] << 3) | ((Buffer[5] & 0b00001100) >> 1);
    crsf->PackedRCdataOut.ch3 = (Buffer[4] << 3) | ((Buffer[5] & 0b00000011) << 1);

    // The low latency switch
    crsf->PackedRCdataOut.ch4 = BIT_to_CRSF((Buffer[6] & 0b01000000) >> 6);

    // The round-robin switch, switchIndex is actually index-1
    // to leave the low bit open for switch 7 (sent as 0b11x)
    // where x is the high bit of switch 7
    uint8_t switchIndex = (Buffer[6] & 0b111000) >> 3;
    uint16_t switchValue = SWITCH3b_to_CRSF(Buffer[6] & 0b111);

    switch (switchIndex) {
        case 0:
            crsf->PackedRCdataOut.ch5 = switchValue;
            break;
        case 1:
            crsf->PackedRCdataOut.ch6 = switchValue;
            break;
        case 2:
            crsf->PackedRCdataOut.ch7 = switchValue;
            break;
        case 3:
            crsf->PackedRCdataOut.ch8 = switchValue;
            break;
        case 4:
            crsf->PackedRCdataOut.ch9 = switchValue;
            break;
        case 5:
            crsf->PackedRCdataOut.ch10 = switchValue;
            break;
        case 6:   // Because AUX1 (index 0) is the low latency switch, the low bit
        case 7:   // of the switchIndex can be used as data, and arrives as index "6"
            crsf->PackedRCdataOut.ch11 = N_to_CRSF(Buffer[6] & 0b1111, 15);
            break;
    }
}
#endif
