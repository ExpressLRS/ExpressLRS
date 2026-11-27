#define CANARD_DSDLC_INTERNAL
#include "uavcan.protocol.SoftwareVersion.h"
#include <string.h>

#ifdef CANARD_DSDLC_TEST_BUILD
#include <test_helpers.h>
#endif

uint32_t uavcan_protocol_SoftwareVersion_encode(struct uavcan_protocol_SoftwareVersion* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
) {
    uint32_t bit_ofs = 0;
    memset(buffer, 0, UAVCAN_PROTOCOL_SOFTWAREVERSION_MAX_SIZE);
    _uavcan_protocol_SoftwareVersion_encode(buffer, &bit_ofs, msg, 
#if CANARD_ENABLE_TAO_OPTION
    tao
#else
    true
#endif
    );
    return ((bit_ofs+7)/8);
}

/*
  return true if the decode is invalid
 */
bool uavcan_protocol_SoftwareVersion_decode(const CanardRxTransfer* transfer, struct uavcan_protocol_SoftwareVersion* msg) {
#if CANARD_ENABLE_TAO_OPTION
    if (transfer->tao && (transfer->payload_len > UAVCAN_PROTOCOL_SOFTWAREVERSION_MAX_SIZE)) {
        return true; /* invalid payload length */
    }
#endif
    uint32_t bit_ofs = 0;
    if (_uavcan_protocol_SoftwareVersion_decode(transfer, &bit_ofs, msg,
#if CANARD_ENABLE_TAO_OPTION
    transfer->tao
#else
    true
#endif
    )) {
        return true; /* invalid payload */
    }

    const uint32_t byte_len = (bit_ofs+7U)/8U;
#if CANARD_ENABLE_TAO_OPTION
    // if this could be CANFD then the dlc could indicating more bytes than
    // we actually have
    if (!transfer->tao) {
        return byte_len > transfer->payload_len;
    }
#endif
    return byte_len != transfer->payload_len;
}

#ifdef CANARD_DSDLC_TEST_BUILD
struct uavcan_protocol_SoftwareVersion sample_uavcan_protocol_SoftwareVersion_msg(void) {
    struct uavcan_protocol_SoftwareVersion msg;

    msg.major = (uint8_t)random_bitlen_unsigned_val(8);
    msg.minor = (uint8_t)random_bitlen_unsigned_val(8);
    msg.optional_field_flags = (uint8_t)random_bitlen_unsigned_val(8);
    msg.vcs_commit = (uint32_t)random_bitlen_unsigned_val(32);
    msg.image_crc = (uint64_t)random_bitlen_unsigned_val(64);
    return msg;
}
#endif
