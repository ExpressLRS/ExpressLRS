#include "SerialSatellite.h"
#include "CRSF.h"
#include "config.h"
#include "device.h"

#if defined(TARGET_RX)

static uint16_t crsfToSatellite(uint16_t crsfValue, uint8_t satelliteChannel, eSerialProtocol protocol)
{
    static constexpr uint16_t SATELLITE_MIN_US = 903;
    static constexpr uint16_t SATELLITE_MAX_US = 2097;

    const bool isDsm2_22ms = (protocol == PROTOCOL_SPEKTRUM_REMOTE_DSM2_22MS);

    // Map the channel data.
    const uint16_t us = constrain(CRSF_to_US(crsfValue), SATELLITE_MIN_US, SATELLITE_MAX_US);
    const float divisor = isDsm2_22ms ? 1.166f : 0.583f;

    uint16_t channelValue = roundf((us - SATELLITE_MIN_US) / divisor);

    // Encode the channel information.
    channelValue |= satelliteChannel << (isDsm2_22ms ? 10 : 11);

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return __builtin_bswap16(channelValue);
#else
    return channelValue;
#endif // __BYTE_ORDER__
}

uint32_t SerialSatellite::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    const eSerialProtocol protocol = config.GetSerialProtocol();
    const uint32_t callbackIntervalMs =
        ((protocol == PROTOCOL_SPEKTRUM_REMOTE_DSMX_11MS) ||
         (protocol == PROTOCOL_SPEKTRUM_REMOTE_DSM2_11MS))
            ? 11
            : 22;

    if ((failsafe && config.GetFailsafeMode() == FAILSAFE_NO_PULSES) ||
        (!sendPackets && connectionState != connected) ||
        frameMissed)
    {
        // Fade count does not overflow.
        if (sendPackets && (fadeCount < UINT8_MAX))
        {
            ++fadeCount;
        }
        return callbackIntervalMs;
    }
    sendPackets = true;

    uint16_t outgoingPacket[SATELLITE_CHANNELS_PER_PACKET];

    // Our bandwidth is limited, so try to send updates only for the channels
    // with new values to reduce the channel update latency. Updates are
    // checked in round-robin fashion to make sure that the channels do not
    // starve.
    for (uint8_t channelCount = 0, packetDataCount = 0;
         (channelCount < SATELLITE_MAX_CHANNELS) && (packetDataCount < SATELLITE_CHANNELS_PER_PACKET);
         ++channelCount)
    {
        // If the channel data is updated, add it to the packet. Or, if the
        // space remaining in the packet is equal to the number of remaining
        // channels to be checked for updates, just fill the packet with the
        // remaining channels no matter if they have updates or not because we
        // need to send exactly 7 channel data in every packet.
        if ((prevChannelData[roundRobinIndex] != channelData[roundRobinIndex]) ||
            ((SATELLITE_CHANNELS_PER_PACKET - packetDataCount) == (SATELLITE_MAX_CHANNELS - channelCount)))
        {
            prevChannelData[roundRobinIndex] = channelData[roundRobinIndex];
            outgoingPacket[packetDataCount++] =
                crsfToSatellite(channelData[roundRobinIndex],
                                roundRobinIndex,
                                protocol);
        }

        ++roundRobinIndex;
        if (roundRobinIndex >= SATELLITE_MAX_CHANNELS)
        {
            roundRobinIndex = 0;
        }
    }

    uint8_t protocolValue{};
    switch (protocol)
    {
    case PROTOCOL_SPEKTRUM_REMOTE_DSMX_11MS:
        protocolValue = 0xB2;
        break;
    case PROTOCOL_SPEKTRUM_REMOTE_DSMX_22MS:
        protocolValue = 0xA2;
        break;
    case PROTOCOL_SPEKTRUM_REMOTE_DSM2_11MS:
        protocolValue = 0x12;
        break;
    default:
        protocolValue = 0x01;
    }

    // Transmit the fade count.
    _outputPort->write(fadeCount);
    // Transmit the protocol in use.
    _outputPort->write(protocolValue);
    // Transmit the channel data.
    _outputPort->write(reinterpret_cast<uint8_t *>(outgoingPacket), sizeof(outgoingPacket));

    return callbackIntervalMs;
}

#endif // defined(TARGET_RX)
