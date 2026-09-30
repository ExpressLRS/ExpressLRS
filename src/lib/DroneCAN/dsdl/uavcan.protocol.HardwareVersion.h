#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "canard.h"


#define UAVCAN_PROTOCOL_HARDWAREVERSION_MAX_SIZE 274
#define UAVCAN_PROTOCOL_HARDWAREVERSION_SIGNATURE (0xAD5C4C933F4A0C4ULL)


struct uavcan_protocol_HardwareVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t unique_id[16];
    struct { uint8_t len; uint8_t data[255]; }certificate_of_authenticity;
};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t uavcan_protocol_HardwareVersion_encode(struct uavcan_protocol_HardwareVersion* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool uavcan_protocol_HardwareVersion_decode(const CanardRxTransfer* transfer, struct uavcan_protocol_HardwareVersion* msg);

#if defined(CANARD_DSDLC_INTERNAL)
static inline void _uavcan_protocol_HardwareVersion_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_HardwareVersion* msg, bool tao);
static inline bool _uavcan_protocol_HardwareVersion_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_HardwareVersion* msg, bool tao);
void _uavcan_protocol_HardwareVersion_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_HardwareVersion* msg, bool tao) {
    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;

    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->major);
    *bit_ofs += 8;
    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->minor);
    *bit_ofs += 8;
    for (size_t i=0; i < 16; i++) {
        canardEncodeScalar(buffer, *bit_ofs, 8, &msg->unique_id[i]);
        *bit_ofs += 8;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    const uint8_t certificate_of_authenticity_len = msg->certificate_of_authenticity.len > 255 ? 255 : msg->certificate_of_authenticity.len;
#pragma GCC diagnostic pop
    if (!tao) {
        canardEncodeScalar(buffer, *bit_ofs, 8, &certificate_of_authenticity_len);
        *bit_ofs += 8;
    }
    for (size_t i=0; i < certificate_of_authenticity_len; i++) {
        canardEncodeScalar(buffer, *bit_ofs, 8, &msg->certificate_of_authenticity.data[i]);
        *bit_ofs += 8;
    }
}

/*
 decode uavcan_protocol_HardwareVersion, return true on failure, false on success
*/
bool _uavcan_protocol_HardwareVersion_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_HardwareVersion* msg, bool tao) {
    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;
    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->major);
    *bit_ofs += 8;

    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->minor);
    *bit_ofs += 8;

    for (size_t i=0; i < 16; i++) {
        canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->unique_id[i]);
        *bit_ofs += 8;
    }

    if (!tao) {
        canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->certificate_of_authenticity.len);
        *bit_ofs += 8;
    } else {
        msg->certificate_of_authenticity.len = ((transfer->payload_len*8)-*bit_ofs)/8;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    if (msg->certificate_of_authenticity.len > 255) {
        return true; /* invalid value */
    }
#pragma GCC diagnostic pop
    for (size_t i=0; i < msg->certificate_of_authenticity.len; i++) {
        canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->certificate_of_authenticity.data[i]);
        *bit_ofs += 8;
    }

    return false; /* success */
}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct uavcan_protocol_HardwareVersion sample_uavcan_protocol_HardwareVersion_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>
#endif
#endif
