// The simulated TX's own copy of the firmware's OTA code. The Makefile builds lib/OTA/OTA.cpp a second
// time and makes that copy's globals private, so nothing here reads or writes the RX's OTA state.
#pragma once

#include <cstdint>

#include "OTA.h"

void txOtaStart(const uint8_t *uid);
void txOtaSelect(OtaSwitchMode_e mode, uint8_t packetSize, uint8_t nonce);
void txOtaPackChannels(OTA_Packet_s *pkt, const uint32_t *channels);
void txOtaAddCrc(OTA_Packet_s *pkt);
void txOtaUnpackChannels(const OTA_Packet_s *pkt, uint32_t *channels);
