/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * This file provides utilities for packing and unpacking the data to
 * be sent over the radio link.
 */

#include "OTA.h"

#if defined HYBRID_SWITCHES_8 or defined UNIT_TEST

static inline uint8_t ICACHE_RAM_ATTR Switches8NonceToSwitchIndex(uint8_t nonce)
{
    // Returns the sequence (0 to 7, then 0 to 7 rotated left by 1):
    // 0, 1, 2, 3, 4, 5, 6, 7,
    // 1, 2, 3, 4, 5, 6, 7, 0
    // Because telemetry can occur on every 2, 4, 8, 16, 32, 64, 128th packet
    // this makes sure each of the 8 values is sent at least once every 16 packets
    // regardless of the TLM ratio
    // Index 7 also can never fall on a telemetry slot
    return ((nonce & 0b111) + ((nonce >> 3) & 0b1)) % 8;
}

#if TARGET_TX or defined UNIT_TEST

/**
 * Return the OTA value respresentation of the switch contained in ChannelDataIn
 * Switches 1-6 (AUX2-AUX7) are 6 or 7 bit depending on the lowRes parameter
 */
static uint8_t ICACHE_RAM_ATTR Switches8ChannelToOta(CRSF *crsf, uint8_t switchIdx, bool lowRes)
{
    uint16_t ch = crsf->ChannelDataIn[switchIdx + 4];
    uint8_t binCount = (lowRes) ? 64 : 128;
    ch = CRSF_to_N(ch, binCount);
    if (lowRes)
        return ch & 0b111111; // 6-bit
    else
        return ch & 0b1111111; // 7-bit
}

/**
 * Hybrid switches packet encoding for sending over the air
 *
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 6 or 7 bit switch value is used to send the remaining switches
 * in a round-robin fashion.
 *
 * Inputs: crsf.ChannelDataIn, crsf.LinkStatistics.uplink_TX_Power
 * Outputs: Radio.TXdataBuffer
 **/
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf,
    uint8_t nonce, uint8_t tlmDenom, bool TelemetryStatus)
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

  uint8_t telemBit = TelemetryStatus << 6;
  uint8_t nextSwitchIndex = Switches8NonceToSwitchIndex(nonce);
  uint8_t value;
  // Using index 7 means the telemetry bit will always be sent in the packet
  // preceding the RX's telemetry slot for all tlmDenom >= 8
  // For more frequent telemetry rates, include the bit in every
  // packet and degrade the value to 6-bit
  // (technically we could squeeze 7-bits in for 2 channels with tlmDenom=4)
  if (nextSwitchIndex == 7)
  {
      value = telemBit | crsf->LinkStatistics.uplink_TX_Power;
  }
  else
  {
      bool telemInEveryPacket = (tlmDenom < 8);
      value = Switches8ChannelToOta(crsf, nextSwitchIndex + 1, telemInEveryPacket);
      if (telemInEveryPacket)
          value |= telemBit;
  }

  Buffer[6] =
      // switch 0 is one bit sent on every packet - intended for low latency arm/disarm
      CRSF_to_BIT(crsf->ChannelDataIn[4]) << 7 |
      // include the switch value
      value;

}
#endif

#if TARGET_RX or defined UNIT_TEST
/**
 * Hybrid switches decoding of over the air data
 *
 * Hybrid switches uses 10 bits for each analog channel,
 * 1 bits for the low latency switch[0]
 * 6 or 7 bits for the round-robin switch
 * 1 bit for the TelemetryStatus, which may be in every packet or just idx 7
 * depending on TelemetryRatio
 *
 * Output: crsf.PackedRCdataOut, crsf.LinkStatistics.uplink_TX_Power
 * Returns: TelemetryStatus bit
 */
bool ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf, uint8_t nonce, uint8_t tlmDenom)
{
    static bool TelemetryStatus = false;

    // The analog channels
    crsf->PackedRCdataOut.ch0 = (Buffer[1] << 3) | ((Buffer[5] & 0b11000000) >> 5);
    crsf->PackedRCdataOut.ch1 = (Buffer[2] << 3) | ((Buffer[5] & 0b00110000) >> 3);
    crsf->PackedRCdataOut.ch2 = (Buffer[3] << 3) | ((Buffer[5] & 0b00001100) >> 1);
    crsf->PackedRCdataOut.ch3 = (Buffer[4] << 3) | ((Buffer[5] & 0b00000011) << 1);

    // The low latency switch (AUX1)
    crsf->PackedRCdataOut.ch4 = BIT_to_CRSF((Buffer[6] & 0b10000000) >> 7);

    // The round-robin switch, 6-7 bits with the switch index implied by the nonce
    uint8_t switchIndex = Switches8NonceToSwitchIndex(nonce);
    bool telemInEveryPacket = (tlmDenom < 8);
    if (telemInEveryPacket || switchIndex == 7)
          TelemetryStatus = (Buffer[6] & 0b01000000) >> 6;
    if (switchIndex == 7)
    {
        crsf->LinkStatistics.uplink_TX_Power = Buffer[6] & 0b111111;
    }
    else
    {
        uint8_t bins;
        uint16_t switchValue;;
        if (telemInEveryPacket)
        {
            bins = 63;
            switchValue = Buffer[6] & 0b111111; // 6-bit
        }
        else
        {
            bins = 127;
            switchValue = Buffer[6] & 0b1111111; // 7-bit
        }

        switchValue = N_to_CRSF(switchValue, bins);
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
            case 6:
                crsf->PackedRCdataOut.ch11 = switchValue;
                break;
        }
    }
    return TelemetryStatus;
}

#endif
#endif // HYBRID_SWITCHES_8

#if !defined HYBRID_SWITCHES_8 or defined UNIT_TEST

#if TARGET_TX or defined UNIT_TEST

void ICACHE_RAM_ATTR GenerateChannelData10bit(volatile uint8_t* Buffer, CRSF *crsf)
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
  Buffer[6] = CRSF_to_BIT(crsf->ChannelDataIn[4]) << 7;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[5]) << 6;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[6]) << 5;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[7]) << 4;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[8]) << 3;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[9]) << 2;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[10]) << 1;
  Buffer[6] |= CRSF_to_BIT(crsf->ChannelDataIn[11]) << 0;
}
#endif

#if TARGET_RX or defined UNIT_TEST

bool ICACHE_RAM_ATTR UnpackChannelData10bit(volatile uint8_t* Buffer, CRSF *crsf)
{
    crsf->PackedRCdataOut.ch0 = (Buffer[1] << 3) | ((Buffer[5] & 0b11000000) >> 5);
    crsf->PackedRCdataOut.ch1 = (Buffer[2] << 3) | ((Buffer[5] & 0b00110000) >> 3);
    crsf->PackedRCdataOut.ch2 = (Buffer[3] << 3) | ((Buffer[5] & 0b00001100) >> 1);
    crsf->PackedRCdataOut.ch3 = (Buffer[4] << 3) | ((Buffer[5] & 0b00000011) << 1);
    crsf->PackedRCdataOut.ch4 = BIT_to_CRSF(Buffer[6] & 0b10000000);
    crsf->PackedRCdataOut.ch5 = BIT_to_CRSF(Buffer[6] & 0b01000000);
    crsf->PackedRCdataOut.ch6 = BIT_to_CRSF(Buffer[6] & 0b00100000);
    crsf->PackedRCdataOut.ch7 = BIT_to_CRSF(Buffer[6] & 0b00010000);
    crsf->PackedRCdataOut.ch8 = BIT_to_CRSF(Buffer[6] & 0b00001000);
    crsf->PackedRCdataOut.ch9 = BIT_to_CRSF(Buffer[6] & 0b00000100);
    crsf->PackedRCdataOut.ch10 = BIT_to_CRSF(Buffer[6] & 0b00000010);
    crsf->PackedRCdataOut.ch11 = BIT_to_CRSF(Buffer[6] & 0b00000001);

    return false;
}

#endif

#endif // !HYBRID_SWITCHES_8
