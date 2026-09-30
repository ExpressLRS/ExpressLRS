#define CANARD_DSDLC_INTERNAL
#include "uavcan.tunnel.Targetted.h"
#include <string.h>

#ifdef CANARD_DSDLC_TEST_BUILD
#include <test_helpers.h>
#endif

uint32_t uavcan_tunnel_Targetted_encode(struct uavcan_tunnel_Targetted* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
) {
    uint32_t bit_ofs = 0;
    memset(buffer, 0, UAVCAN_TUNNEL_TARGETTED_MAX_SIZE);
    _uavcan_tunnel_Targetted_encode(buffer, &bit_ofs, msg, 
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
bool uavcan_tunnel_Targetted_decode(const CanardRxTransfer* transfer, struct uavcan_tunnel_Targetted* msg) {
#if CANARD_ENABLE_TAO_OPTION
    if (transfer->tao && (transfer->payload_len > UAVCAN_TUNNEL_TARGETTED_MAX_SIZE)) {
        return true; /* invalid payload length */
    }
#endif
    uint32_t bit_ofs = 0;
    if (_uavcan_tunnel_Targetted_decode(transfer, &bit_ofs, msg,
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
struct uavcan_tunnel_Targetted sample_uavcan_tunnel_Targetted_msg(void) {
    struct uavcan_tunnel_Targetted msg;

    msg.protocol = sample_uavcan_tunnel_Protocol_msg();
    msg.target_node = (uint8_t)random_bitlen_unsigned_val(7);
    msg.serial_id = (int8_t)random_bitlen_signed_val(5);
    msg.options = (uint8_t)random_bitlen_unsigned_val(4);
    msg.baudrate = (uint32_t)random_bitlen_unsigned_val(24);
    msg.buffer.len = (uint8_t)random_range_unsigned_val(0, 120);
    for (size_t i=0; i < msg.buffer.len; i++) {
        msg.buffer.data[i] = (uint8_t)random_bitlen_unsigned_val(8);
    }
    return msg;
}
#endif
