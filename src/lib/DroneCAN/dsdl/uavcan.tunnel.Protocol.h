#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "canard.h"


#define UAVCAN_TUNNEL_PROTOCOL_MAX_SIZE 1
#define UAVCAN_TUNNEL_PROTOCOL_SIGNATURE (0xA367483C9B920E49ULL)

#define UAVCAN_TUNNEL_PROTOCOL_MAVLINK 0
#define UAVCAN_TUNNEL_PROTOCOL_MAVLINK2 1
#define UAVCAN_TUNNEL_PROTOCOL_GPS_GENERIC 2
#define UAVCAN_TUNNEL_PROTOCOL_UNDEFINED 255


struct uavcan_tunnel_Protocol {
    uint8_t protocol;
};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t uavcan_tunnel_Protocol_encode(struct uavcan_tunnel_Protocol* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool uavcan_tunnel_Protocol_decode(const CanardRxTransfer* transfer, struct uavcan_tunnel_Protocol* msg);

#if defined(CANARD_DSDLC_INTERNAL)
static inline void _uavcan_tunnel_Protocol_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_tunnel_Protocol* msg, bool tao);
static inline bool _uavcan_tunnel_Protocol_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_tunnel_Protocol* msg, bool tao);
void _uavcan_tunnel_Protocol_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_tunnel_Protocol* msg, bool tao) {
    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;

    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->protocol);
    *bit_ofs += 8;
}

/*
 decode uavcan_tunnel_Protocol, return true on failure, false on success
*/
bool _uavcan_tunnel_Protocol_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_tunnel_Protocol* msg, bool tao) {
    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;
    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->protocol);
    *bit_ofs += 8;

    return false; /* success */
}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct uavcan_tunnel_Protocol sample_uavcan_tunnel_Protocol_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>
#endif
#endif
