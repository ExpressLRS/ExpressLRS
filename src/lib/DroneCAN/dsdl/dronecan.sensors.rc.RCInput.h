#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "canard.h"


#define DRONECAN_SENSORS_RC_RCINPUT_MAX_SIZE 53
#define DRONECAN_SENSORS_RC_RCINPUT_SIGNATURE (0x771555E596AAB4CFULL)
#define DRONECAN_SENSORS_RC_RCINPUT_ID 1140

#define DRONECAN_SENSORS_RC_RCINPUT_STATUS_QUALITY_VALID 1
#define DRONECAN_SENSORS_RC_RCINPUT_STATUS_FAILSAFE 2
#define DRONECAN_SENSORS_RC_RCINPUT_STATUS_QUALITY_TYPE 28
#define DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_RSSI 0
#define DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_LQ_ACTIVE_ANTENNA 4
#define DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_RSSI_DBM 8
#define DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_SNR 12
#define DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_TX_POWER 16

#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
class dronecan_sensors_rc_RCInput_cxx_iface;
#endif

struct dronecan_sensors_rc_RCInput {
#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
    using cxx_iface = dronecan_sensors_rc_RCInput_cxx_iface;
#endif
    uint16_t status;
    uint8_t quality;
    uint8_t id;
    struct { uint8_t len; uint16_t data[32]; }rcin;
};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t dronecan_sensors_rc_RCInput_encode(struct dronecan_sensors_rc_RCInput* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool dronecan_sensors_rc_RCInput_decode(const CanardRxTransfer* transfer, struct dronecan_sensors_rc_RCInput* msg);

#if defined(CANARD_DSDLC_INTERNAL)
static inline void _dronecan_sensors_rc_RCInput_encode(uint8_t* buffer, uint32_t* bit_ofs, struct dronecan_sensors_rc_RCInput* msg, bool tao);
static inline bool _dronecan_sensors_rc_RCInput_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct dronecan_sensors_rc_RCInput* msg, bool tao);
void _dronecan_sensors_rc_RCInput_encode(uint8_t* buffer, uint32_t* bit_ofs, struct dronecan_sensors_rc_RCInput* msg, bool tao) {
    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;

    canardEncodeScalar(buffer, *bit_ofs, 16, &msg->status);
    *bit_ofs += 16;
    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->quality);
    *bit_ofs += 8;
    canardEncodeScalar(buffer, *bit_ofs, 4, &msg->id);
    *bit_ofs += 4;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    const uint8_t rcin_len = msg->rcin.len > 32 ? 32 : msg->rcin.len;
#pragma GCC diagnostic pop
    if (!tao) {
        canardEncodeScalar(buffer, *bit_ofs, 6, &rcin_len);
        *bit_ofs += 6;
    }
    for (size_t i=0; i < rcin_len; i++) {
        canardEncodeScalar(buffer, *bit_ofs, 12, &msg->rcin.data[i]);
        *bit_ofs += 12;
    }
}

/*
 decode dronecan_sensors_rc_RCInput, return true on failure, false on success
*/
bool _dronecan_sensors_rc_RCInput_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct dronecan_sensors_rc_RCInput* msg, bool tao) {
    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;
    canardDecodeScalar(transfer, *bit_ofs, 16, false, &msg->status);
    *bit_ofs += 16;

    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->quality);
    *bit_ofs += 8;

    canardDecodeScalar(transfer, *bit_ofs, 4, false, &msg->id);
    *bit_ofs += 4;

    if (!tao) {
        canardDecodeScalar(transfer, *bit_ofs, 6, false, &msg->rcin.len);
        *bit_ofs += 6;
    } else {
        msg->rcin.len = ((transfer->payload_len*8)-*bit_ofs)/12;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    if (msg->rcin.len > 32) {
        return true; /* invalid value */
    }
#pragma GCC diagnostic pop
    for (size_t i=0; i < msg->rcin.len; i++) {
        canardDecodeScalar(transfer, *bit_ofs, 12, false, &msg->rcin.data[i]);
        *bit_ofs += 12;
    }

    return false; /* success */
}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct dronecan_sensors_rc_RCInput sample_dronecan_sensors_rc_RCInput_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>
BROADCAST_MESSAGE_CXX_IFACE(dronecan_sensors_rc_RCInput, DRONECAN_SENSORS_RC_RCINPUT_ID, DRONECAN_SENSORS_RC_RCINPUT_SIGNATURE, DRONECAN_SENSORS_RC_RCINPUT_MAX_SIZE);
#endif
#endif
