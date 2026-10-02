// The checks made on every RC frame the RX emits, reported as VIOLATION <name>:
//   leak          a channel the TX transmits was output before it was ever received, without failsafe flagged
//   wrong-value   a received channel was output with a value other than what the TX sent
//   unset-not-min a channel the TX never transmits was output as something other than minimum
//   blackout      valid RC packets on a healthy link produce no output
//   protocol-changed the RX's configured serial protocol was changed by something received over the air
// wrong-value is off once a damaged packet has passed the CRC, as any value can arrive then.
#pragma once

constexpr unsigned BLACKOUT_PACKETS = 40;  // valid packets, no output -> bug

enum Proto { PROTO_CRSF, PROTO_SBUS, PROTO_SUMD, PROTO_COUNT };
extern const char *const protoNames[];

void checksStart(Proto outputProtocol);
void checksCrcCollision();
void checkOutput();
void checkRcPacket();
void checkProtocol();
