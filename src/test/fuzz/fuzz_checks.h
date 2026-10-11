// The checks made on every RC frame the RX emits, reported as VIOLATION <name>:
//   leak                  a channel the TX transmits was output before it was ever received,
//                         without failsafe flagged
//   wrong-value           a received channel was output with a value other than the last one
//                         received for it
//   unset-not-min         a channel the TX never transmits was output as something other than
//                         minimum
//   blackout              valid RC packets on a healthy link produce no output
//   failsafe-not-flagged  the link is lost, but the RX keeps sending RC frames that are not flagged
//                         failsafe. CRSF has no such flag, so there it is any RC frame.
//   failsafe-not-held     the RX is set to hold the last position on failsafe, but after losing the
//                         link it output a channel at some other value than the last one it had
//   protocol-changed      the RX's configured serial protocol was changed by something received
//                         over the air
// wrong-value is off once a damaged packet has passed the CRC, as any value can arrive then.
#pragma once

#include "common.h"
#include "OTA.h"

constexpr unsigned BLACKOUT_PACKETS = 40;  // valid packets, no output -> bug

enum Proto { PROTO_CRSF, PROTO_SBUS, PROTO_SUMD, PROTO_COUNT };
extern const char *const protoNames[];
extern const eSerialProtocol protoConfigValues[]; // what the RX is configured with for each

void checksStart(Proto outputProtocol);
void checksCrcCollision();
void checkRxFrames();
void checkRcPacket(const OTA_Packet_s *pkt, bool asSent);
void checkProtocol();
