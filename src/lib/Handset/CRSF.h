#ifndef H_CRSF
#define H_CRSF

#include "crsf_protocol.h"
#include "telemetry_protocol.h"
#include "msp.h"
#include "msptypes.h"

class CRSF
{
public:
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statistics Stored as Struct

    static void GetMspMessage(uint8_t **data, uint8_t *len);
    static void UnlockMspMessage();
    static void AddMspMessage(uint8_t length, uint8_t *data);
    static void AddMspMessage(mspPacket_t *packet, uint8_t destination);
    static void ResetMspQueue();

    static void GetDeviceInformation(uint8_t *frame, uint8_t fieldCount);
    static void SetMspV2Request(uint8_t *frame, uint16_t function, uint8_t *payload, uint8_t payloadLength);
    static void SetHeaderAndCrc(uint8_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);
    static void SetExtendedHeaderAndCrc(uint8_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e senderAddr, crsf_addr_e destAddr);
    static uint32_t VersionStrToU32(const char *verStr);

#if defined(CRSF_RX_MODULE)
public:
    static void updateUplinkPower(uint8_t uplinkPower);
    static bool clearUpdatedUplinkPower();

private:
    static bool HasUpdatedUplinkPower;
#endif

private:
    static uint8_t MspData[ELRS_MSP_BUFFER];
    static uint8_t MspDataLength;
};

extern GENERIC_CRC8 crsf_crc;

#endif