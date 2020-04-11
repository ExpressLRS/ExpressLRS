/**
 * TODO - add header
 */

#include "OTA.h"

#ifdef HYBRID_SWITCHES_8

/**
 * Hybrid switches packet
 * Replaces Generate4ChannelData_11bit
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 2 bit value is used to send the remaining switches
 * in a round-robin fashion.
 * If any of the round-robin switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 * 
 * Inputs: crsf.ChannelDataIn, crsf.currentSwitches
 * Outputs: Radio.TXdataBuffer, side-effects the sentSwitch value
 */
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(SX127xDriver *Radio, CRSF *crsf, uint8_t addr)
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (addr << 2) + RC_DATA_PACKET;
  Radio->TXdataBuffer[0] = PacketHeaderAddr;
  Radio->TXdataBuffer[1] = ((crsf->ChannelDataIn[0]) >> 3);
  Radio->TXdataBuffer[2] = ((crsf->ChannelDataIn[1]) >> 3);
  Radio->TXdataBuffer[3] = ((crsf->ChannelDataIn[2]) >> 3);
  Radio->TXdataBuffer[4] = ((crsf->ChannelDataIn[3]) >> 3);
  Radio->TXdataBuffer[5] = ((crsf->ChannelDataIn[0] & 0b110) << 5) + 
                           ((crsf->ChannelDataIn[1] & 0b110) << 3) +
                           ((crsf->ChannelDataIn[2] & 0b110) << 1) + 
                           ((crsf->ChannelDataIn[3] & 0b110) >> 1);

  // switch 0 is sent on every packet - intended for low latency arm/disarm
  Radio->TXdataBuffer[6] = (crsf->currentSwitches[0] & 0b11) << 5; // note this leaves the top bit of byte 6 unused

  // find the next switch to send
  int i = crsf->getNextSwitchIndex() & 0b111;      // mask for paranoia
  uint8_t value = crsf->currentSwitches[i] & 0b11; // mask for paranoia

  // put the bits into buf[6]-> i is in the range 1 through 7 so takes 3 bits
  // currentSwitches[i] is in the range 0 through 2, takes 2 bits->
  Radio->TXdataBuffer[6] += (i << 2) + value;

  // update the sent value
  crsf->setSentSwitch(i, value);
}
#endif
