#include "sim_tx_ota.h"

#include <cstring>

#define TX_OTA_API __attribute__((visibility("default")))

// Binds the TX to the RX's UID
TX_OTA_API void txOtaStart(const uint8_t *uid)
{
    memcpy(UID, uid, UID_LEN);
    OtaUpdateCrcInitFromUid();
}

// Sets what the next packet is packed and checksummed with
TX_OTA_API void txOtaSelect(OtaSwitchMode_e mode, uint8_t packetSize, uint8_t nonce)
{
    OtaUpdateSerializers(mode, packetSize);
    OtaNonce = nonce;
    isArmed = true;
}

// Packs an RC packet
TX_OTA_API void txOtaPackChannels(OTA_Packet_s *pkt, const uint32_t *channels)
{
    OtaPackChannelData(pkt, channels, false);
}

// Adds the CRC to a finished packet
TX_OTA_API void txOtaAddCrc(OTA_Packet_s *pkt)
{
    OtaGeneratePacketCrc(pkt);
}

// Unpacks an RC packet the way a perfect RX would, without touching the RX under test
TX_OTA_API void txOtaUnpackChannels(const OTA_Packet_s *pkt, uint32_t *channels)
{
    OtaUnpackChannelData(pkt, channels);
}
