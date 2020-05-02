#include "CRSF_RX.h"
#include "utils.h"
#include "debug.h"
#include <string.h>

void ICACHE_RAM_ATTR CRSF_RX::sendFrameToFC(uint8_t *buff, uint8_t size)
{
    buff[size - 1] = CalcCRC(&buff[2], (buff[1] - 1));
#if !NO_DATA_TO_FC
    _dev->write(buff, size);
#endif
}

void CRSF_RX::LinkStatisticsSend()
{
    uint8_t len = CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength);

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(&outBuffer[3], (void *)&LinkStatistics, LinkStatisticsFrameLength);

    sendFrameToFC(outBuffer, len);
}

void ICACHE_RAM_ATTR CRSF_RX::sendRCFrameToFC()
{
    uint8_t len = CRSF_EXT_FRAME_SIZE(RCframeLength);

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = CRSF_FRAME_SIZE(RCframeLength);
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memcpy(&outBuffer[3], &ChannelsPacked, RCframeLength);

    sendFrameToFC(outBuffer, len);
}

void ICACHE_RAM_ATTR CRSF_RX::sendMSPFrameToFC(mspPacket_t& packet)
{
    // TODO: This currently only supports single MSP packets per cmd
    // To support longer packets we need to re-write this to allow packet splitting

    uint8_t i;
    uint8_t msp_len = CRSF_MSP_FRAME_SIZE(packet.payloadSize);
    uint8_t len = CRSF_EXT_FRAME_SIZE(msp_len); // total len

    // CRSF extended frame header
    outBuffer[0] = CRSF_ADDRESS_BROADCAST; // address
    outBuffer[1] = CRSF_FRAME_SIZE(msp_len); // CRSF frame len
    outBuffer[2] = CRSF_FRAMETYPE_MSP_WRITE; // packet type
    outBuffer[3] = CRSF_ADDRESS_FLIGHT_CONTROLLER; // destination
    outBuffer[4] = CRSF_ADDRESS_RADIO_TRANSMITTER; // origin

    // Encapsulated MSP payload
    outBuffer[5] = 0x30; // header
    outBuffer[6] = packet.payloadSize; // mspPayloadSize
    outBuffer[7] = packet.function;
    for (i = 8; i < (packet.payloadSize + 8); ++i) {
        // copy packet payload into outBuffer and pad with zeros where required
        outBuffer[i] = packet.payload[i];
    }
    // Encapsulated MSP crc
    outBuffer[i] =  CalcCRCMsp(&outBuffer[6], (packet.payloadSize + 1 /*func*/));

    sendFrameToFC(outBuffer, len);
}

void CRSF_RX::processPacket(uint8_t const *data)
{
    switch (data[0])
    {
        case CRSF_FRAMETYPE_COMMAND:
        {
            if (data[1] == 0x62 && data[2] == 0x6c)
            {
                DEBUG_PRINTLN("Jumping to Bootloader...");
                delay(200);
                platform_restart();
            }
            break;
        }

        case CRSF_FRAMETYPE_BATTERY_SENSOR:
        {
            TLMbattSensor.voltage = data[1];
            TLMbattSensor.voltage <<= 8;
            TLMbattSensor.voltage = data[2];

            TLMbattSensor.current = data[3];
            TLMbattSensor.current <<= 8;
            TLMbattSensor.current = data[4];

            TLMbattSensor.capacity = data[5];
            TLMbattSensor.capacity <<= 8;
            TLMbattSensor.capacity = data[6];
            TLMbattSensor.capacity <<= 8;
            TLMbattSensor.capacity = data[7];

            TLMbattSensor.remaining = 0;
            break;
        }

        default:
            break;
    }
}

void CRSF_RX::handleUartIn(volatile uint8_t &rx_data_rcvd)
{
    uint8_t split_cnt = 0;

    while (rx_data_rcvd == 0 && _dev->available() && ((++split_cnt & 0xF) > 0))
    {
        uint8_t *ptr = HandleUartIn(_dev->read());
        if (ptr)
            processPacket(ptr);
    }
}
