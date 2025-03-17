#include "SerialIBus.h"
#include "CRSF.h"
#include "device.h"
#include "config.h"

#if defined(TARGET_RX)

const auto UNCONNECTED_CALLBACK_INTERVAL_MS = 10;

uint32_t SerialIBus::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    // Using the same logic as SBUS to decide whether or not to send packets
    static auto sendPackets = false;
    bool effectivelyFailsafed = failsafe || (!connectionHasModelMatch) || (!teamraceHasModelMatch);
    if ((effectivelyFailsafed && config.GetFailsafeMode() == FAILSAFE_NO_PULSES) || (!sendPackets && connectionState != connected))
    {
        return UNCONNECTED_CALLBACK_INTERVAL_MS;
    }
    sendPackets = true;

    if ((!frameAvailable && !frameMissed && !effectivelyFailsafed) || _outputPort->availableForWrite() < sizeof(ibus_channels_packet_t))
    {
        return DURATION_IMMEDIATELY;
    }

    // TODO: if failsafeMode == FAILSAFE_SET_POSITION then we use the set positions rather than the last values
    ibus_channels_packet_t PackedRCdataOut;

    PackedRCdataOut.checksum = IBUS_CHECKSUM_START; // Compute checksum as we go
    PackedRCdataOut.header = IBUS_HEADER;
    PackedRCdataOut.checksum -= PackedRCdataOut.header;

    for(int i = 0; i < IBUS_NUM_CHANS; i++)
    {
        uint16_t channel_output = fmap(channelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, IBUS_CHANNEL_VALUE_MIN, IBUS_CHANNEL_VALUE_MAX);
        PackedRCdataOut.channels[i] = channel_output;
        PackedRCdataOut.checksum -= PackedRCdataOut.channels[i];
    }

    _outputPort->write((byte *)&PackedRCdataOut, sizeof(PackedRCdataOut));
    return IBUS_CHAN_PACKET_INTERVAL_MS;
}

void SerialIBus::setChecksum(ibus_channels_packet_t *packet)
{
    // Recalculate packet checksum if necessary
    packet->checksum = IBUS_CHECKSUM_START;

    // Subtract every two bytes before the checksum as if they were uint16_ts
    for(int i = 0; i < offsetof(ibus_channels_packet_t, checksum) / 2; i++)
    {
        packet->checksum -= ((uint16_t *) packet)[i];
    }
}

#endif
