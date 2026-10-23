#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "canard.h"


#define UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_MAX_SIZE 0
#define UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_SIGNATURE (0xEE468A8121C46A9EULL)
#define UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_ID 1

#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
class uavcan_protocol_GetNodeInfo_cxx_iface;
#endif

struct uavcan_protocol_GetNodeInfoRequest {
#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
    using cxx_iface = uavcan_protocol_GetNodeInfo_cxx_iface;
#endif
};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t uavcan_protocol_GetNodeInfoRequest_encode(struct uavcan_protocol_GetNodeInfoRequest* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool uavcan_protocol_GetNodeInfoRequest_decode(const CanardRxTransfer* transfer, struct uavcan_protocol_GetNodeInfoRequest* msg);

#if defined(CANARD_DSDLC_INTERNAL)
static inline void _uavcan_protocol_GetNodeInfoRequest_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_GetNodeInfoRequest* msg, bool tao);
static inline bool _uavcan_protocol_GetNodeInfoRequest_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_GetNodeInfoRequest* msg, bool tao);
void _uavcan_protocol_GetNodeInfoRequest_encode(uint8_t* buffer, uint32_t* bit_ofs, struct uavcan_protocol_GetNodeInfoRequest* msg, bool tao) {
    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;

}

/*
 decode uavcan_protocol_GetNodeInfoRequest, return true on failure, false on success
*/
bool _uavcan_protocol_GetNodeInfoRequest_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct uavcan_protocol_GetNodeInfoRequest* msg, bool tao) {
    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;
    return false; /* success */
}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct uavcan_protocol_GetNodeInfoRequest sample_uavcan_protocol_GetNodeInfoRequest_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>
#endif
#endif
