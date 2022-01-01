
#pragma once

#define CRSF_MSP_FRAME_OFFSET 6                                             // Start of MSP frame in CRSF packet
#define CRSF_MSP_STATUS_BYTE_OFFSET 5                                       // Status byte index in CRSF packet
#define CRSF_MSP_SRC_OFFSET 4                                               // Source ID index in CRSF packet
#define CRSF_MSP_DEST_OFFSET 3                                              // MSP type index in CRSF packet
#define CRSF_FRAME_PAYLOAD_LEN_IDX 1                                        // Extended frame payload length index in CRSF packet
#define CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET 5                            // For Ext Frame, playload is this much bigger than it says in the crsf frame
#define CRSF_MSP_MAX_BYTES_PER_CHUNK 57                                     // Max bytes per MSP chunk in CRSF packet
#define CRSF_MSP_TYPE_IDX 2                                                 // MSP type index in CRSF packet
#define MSP_FRAME_MAX_LEN 384                                               // Max MSP frame length (increase as needed)
#define CRSF_MSP_OUT_BUFFER_DEPTH (MSP_FRAME_MAX_LEN / CRSF_MAX_PACKET_LEN) // Max number of CRSF frames to buffer

#define CRSF_MSP_LEN_TO_ENCAP_FRAME_OFFSET (CRSF_MAX_PACKET_LEN - CRSF_MSP_MAX_BYTES_PER_CHUNK) // equals 7
// <sync><crsf_len><crsf_cmd><dst><source><header><msp_len><msp_cmd>

#define MSP_V1_FRAME_LEN_FROM_PAYLOAD_LEN(payload_len) ((payload_len) + 2)       // doesn't include $M< header
#define MSP_V1_JUMBO_FRAME_LEN_FROM_PAYLOAD_LEN(payload_len) ((payload_len) + 4) // extra 2 bytes for jumbo frame
#define MSP_V2_FRAME_LEN_FROM_PAYLOAD_LEN(payload_len) ((payload_len) + 5)       // doesn't include $X< header

typedef enum
{
    MSP_FRAME_V1 = 1,
    MSP_FRAME_V2 = 2,
    MSP_FRAME_V1_JUMBO = 3,
    MSP_FRAME_UNKNOWN = 4,
} MSPframeType_e;
