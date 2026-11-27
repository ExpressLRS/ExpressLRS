#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "canard.h"


#define UAVCAN_PROTOCOL_SOFTWAREVERSION_MAX_SIZE 15
#define UAVCAN_PROTOCOL_SOFTWAREVERSION_SIGNATURE (0xDD46FD376527FEA1ULL)

#define UAVCAN_PROTOCOL_SOFTWAREVERSION_OPTIONAL_FIELD_FLAG_VCS_COMMIT 1
#define UAVCAN_PROTOCOL_SOFTWAREVERSION_OPTIONAL_FIELD_FLAG_IMAGE_CRC 2


struct uavcan_protocol_SoftwareVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t optional_field_flags;
    uint32_t vcs_commit;
    uint64_t image_crc;
};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t uavcan_protocol_SoftwareVersion_encode(struct uavcan_protocol_SoftwareVersion* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool uavcan_protocol_SoftwareVersion_decode(const CanardRxTransfer* transfer, struct uavcan_protocol_SoftwareVersion* msg);

#if defined(CANARD_DSDLC_INTERNAL)
static inline void _uavcan_protocol_SoftwareVersion_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_SoftwareVersion* msg, bool tao);
static inline bool _uavcan_protocol_SoftwareVersion_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_SoftwareVersion* msg, bool tao);
void _uavcan_protocol_SoftwareVersion_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_SoftwareVersion* msg, bool tao) {
    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;

    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->major);
    *bit_ofs += 8;
    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->minor);
    *bit_ofs += 8;
    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->optional_field_flags);
    *bit_ofs += 8;
    canardEncodeScalar(buffer, *bit_ofs, 32, &msg->vcs_commit);
    *bit_ofs += 32;
    canardEncodeScalar(buffer, *bit_ofs, 64, &msg->image_crc);
    *bit_ofs += 64;
}

/*
 decode uavcan_protocol_SoftwareVersion, return true on failure, false on success
*/
bool _uavcan_protocol_SoftwareVersion_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_SoftwareVersion* msg, bool tao) {
    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;
    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->major);
    *bit_ofs += 8;

    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->minor);
    *bit_ofs += 8;

    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->optional_field_flags);
    *bit_ofs += 8;

    canardDecodeScalar(transfer, *bit_ofs, 32, false, &msg->vcs_commit);
    *bit_ofs += 32;

    canardDecodeScalar(transfer, *bit_ofs, 64, false, &msg->image_crc);
    *bit_ofs += 64;

    return false; /* success */
}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct uavcan_protocol_SoftwareVersion sample_uavcan_protocol_SoftwareVersion_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>
#endif
#endif
