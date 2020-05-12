#include "CRSF_TX.h"
#include "debug.h"
#include "FIFO.h"
#include <string.h>

void paramNullCallback(uint8_t const *, uint16_t){};
void (*CRSF_TX::ParamWriteCallback)(uint8_t const *msg, uint16_t len) = &paramNullCallback;

///Out FIFO to buffer messages///
FIFO SerialOutFIFO;

void CRSF_TX::Begin(void)
{
    CRSF::Begin();
    p_UartNextCheck = millis(); //  +UARTwdtInterval * 2;
}

void ICACHE_RAM_ATTR CrsfFramePushToFifo(uint8_t *buff, uint8_t size)
{
    buff[size - 1] = CalcCRC(&buff[2], (buff[1] - 1));
    SerialOutFIFO.push(size); // length
    SerialOutFIFO.pushBytes(buff, size);
}

void CRSF_TX::LinkStatisticsSend(void)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength);
    uint8_t buffer[len];

    buffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    buffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // this is nok with volatile
    memcpy(&buffer[3], (void *)&LinkStatistics, LinkStatisticsFrameLength);

    CrsfFramePushToFifo(buffer, len);
}

void CRSF_TX::sendSetVTXchannelToRadio(uint8_t band, uint8_t channel)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(VTXcontrolFrameLength);
    uint8_t buffer[len];

    buffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    buffer[1] = CRSF_FRAME_SIZE(VTXcontrolFrameLength);
    buffer[2] = CRSF_FRAMETYPE_COMMAND;
    buffer[3] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    buffer[4] = CRSF_ADDRESS_CRSF_RECEIVER;
    /////////////////////////////////////////////
    buffer[5] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    buffer[6] = CRSF_ADDRESS_CRSF_RECEIVER;
    buffer[7] = 0x08; // vtx command
    ////////////////////////////////////////////
    buffer[8] = channel + 8 * band;
    buffer[9] = 0x00;
    buffer[10] = 0x00;
    buffer[11] = 0x00;
    buffer[12] = 0x00;
    buffer[13] = 0x00;

    CrsfFramePushToFifo(buffer, len);
}

void CRSF_TX::sendLUAresponseToRadio(uint8_t *data, uint8_t size)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(2 + size);
    uint8_t buffer[len];

    buffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buffer[1] = CRSF_FRAME_SIZE(2 + size);
    buffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    buffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    for (uint8_t i = 0; i < size; i++)
        buffer[5 + i] = data[i];

    CrsfFramePushToFifo(buffer, len);
}

void CRSF_TX::sendMspPacketToRadio(mspPacket_t &msp)
{
    if (!CRSFstate)
        return;

    uint8_t msp_len = CRSF_MSP_FRAME_SIZE(msp.payloadSize);
    uint8_t len = CRSF_EXT_FRAME_SIZE(msp_len);

    if (CRSF_EXT_FRAME_SIZE(CRSF_PAYLOAD_SIZE_MAX) < len)
        // just ignore if too big
        return;

    // CRSF MSP packet
    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(msp_len);
    outBuffer[2] = CRSF_FRAMETYPE_MSP_RESP;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_FLIGHT_CONTROLLER;

    // Encapsulated MSP payload
    outBuffer[5] = msp.flags;       // 0x30, header
    outBuffer[6] = msp.payloadSize; // mspPayloadSize
    outBuffer[7] = msp.function;
    for (uint16_t i = 8; i < (msp.payloadSize + 8); i++)
    {
        // copy packet payload into outBuffer and pad with zeros where required
        outBuffer[i] = msp.payload[i];
    }

    CrsfFramePushToFifo(outBuffer, len);
}

void CRSF_TX::BatterySensorSend(void)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(BattSensorFrameLength);
    uint8_t buffer[len];

    buffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buffer[1] = CRSF_FRAME_SIZE(BattSensorFrameLength);
    buffer[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

    buffer[3] = (TLMbattSensor.voltage >> 8) & 0xff;
    buffer[4] = TLMbattSensor.voltage & 0xff;
    buffer[5] = (TLMbattSensor.current >> 8) & 0xff;
    buffer[6] = TLMbattSensor.current & 0xff;
    buffer[7] = (TLMbattSensor.capacity >> 16) & 0xff;
    buffer[9] = (TLMbattSensor.capacity >> 8) & 0xff;
    buffer[10] = TLMbattSensor.capacity & 0xff;
    buffer[11] = TLMbattSensor.remaining;

    CrsfFramePushToFifo(buffer, len);
}

#if (FEATURE_OPENTX_SYNC)
void CRSF_TX::sendSyncPacketToRadio() // in values in us.
{
    if (RCdataLastRecv && CRSFstate)
    {
        uint32_t current = millis();
        //DEBUG_PRINT(OpenTXsyncOffset);

        if (OpenTXsyncPakcetInterval <= (current - OpenTXsynNextSend))
        {
            uint32_t packetRate = RequestedRCpacketInterval;
            packetRate *= 10; //convert from us to right format

            int32_t offset = OpenTXsyncOffset - RequestedRCpacketAdvance;
            offset *= 10;

            uint8_t len = CRSF_EXT_FRAME_SIZE(OpenTXsyncFrameLength);
            uint8_t buffer[len];

            buffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;         // 0xEA
            buffer[1] = CRSF_FRAME_SIZE(OpenTXsyncFrameLength); // 13
            buffer[2] = CRSF_FRAMETYPE_RADIO_ID;                // 0x3A

            buffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER; //0XEA
            buffer[4] = 0x00;                           //??? not sure doesn't seem to matter
            buffer[5] = CRSF_FRAMETYPE_OPENTX_SYNC;     //0X10

            buffer[6] = (packetRate & 0xFF000000) >> 24;
            buffer[7] = (packetRate & 0x00FF0000) >> 16;
            buffer[8] = (packetRate & 0x0000FF00) >> 8;
            buffer[9] = (packetRate & 0x000000FF) >> 0;

            buffer[10] = (offset & 0xFF000000) >> 24;
            buffer[11] = (offset & 0x00FF0000) >> 16;
            buffer[12] = (offset & 0x0000FF00) >> 8;
            buffer[13] = (offset & 0x000000FF) >> 0;

            CrsfFramePushToFifo(buffer, len);

            OpenTXsynNextSend = current;
            platform_wd_feed();
        }
    }
}
#endif /* FEATURE_OPENTX_SYNC */

void CRSF_TX::processPacket(uint8_t const *input)
{
    if (CRSFstate == false)
    {
        CRSFstate = true;
#if (FEATURE_OPENTX_SYNC)
        RCdataLastRecv = 0;
        OpenTXsynNextSend = millis(); //+60;
#endif
        DEBUG_PRINTLN("CRSF UART Connected");
        connected();
    }

    switch (input[0]) // check CRSF command
    {
        case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
        {
#if (FEATURE_OPENTX_SYNC)
            RCdataLastRecv = micros();
#endif
            (RCdataCallback1)((crsf_channels_t *)&input[1]); // run new RC data callback
            break;
        }
        case CRSF_FRAMETYPE_PARAMETER_WRITE:
        {
            if (input[1] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
                input[2] == CRSF_ADDRESS_RADIO_TRANSMITTER)
            {
                ParamWriteCallback(&input[3], 2);
            }
        }
        case CRSF_FRAMETYPE_MSP_REQ:
        case CRSF_FRAMETYPE_MSP_WRITE:
        {
            if (input[1] == CRSF_ADDRESS_FLIGHT_CONTROLLER &&
                input[2] == CRSF_ADDRESS_RADIO_TRANSMITTER)
            {
                MspCallback(&input[3]); // pointer to MSP packet
            }
            break;
        }
        default:
            break;
    };
}

void CRSF_TX::handleUartIn(volatile uint8_t &rx_data_rcvd) // Merge with RX version...
{
    uint8_t split_cnt = 0;
    if (rx_data_rcvd)
        return;

#if (FEATURE_OPENTX_SYNC)
    sendSyncPacketToRadio();
#endif

    while (rx_data_rcvd == 0 && _dev->available() && ((++split_cnt & 0x7) > 0))
    {
        uint8_t *ptr = HandleUartIn(_dev->read());
        if (ptr)
        {
            processPacket(ptr);
            /* Can write right after successful package reception */
            _dev->write(SerialOutFIFO);
        }
        platform_wd_feed();
    }

    if (rx_data_rcvd == 0)
        uart_wdt();
}

void CRSF_TX::uart_wdt(void)
{
    uint32_t now = millis();
    if (UARTwdtInterval < (now - p_UartNextCheck))
    {
        DEBUG_PRINT("UART RX packets! Bad:Good ");
        DEBUG_PRINT(BadPktsCount);
        DEBUG_PRINT(":");
        DEBUG_PRINTLN(GoodPktsCount);

        if (BadPktsCount >= GoodPktsCount)
        {
            //DEBUG_PRINTLN("  Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                SerialOutFIFO.flush();
                DEBUG_PRINT("CRSF UART Disconnect. ");
                disconnected();
                CRSFstate = false;
#if (FEATURE_OPENTX_SYNC)
                OpenTXsynNextSend = 0;
                OpenTXsyncOffset = 0;
#endif
            }

            _dev->end();
            platform_wd_feed();

            if (p_slowBaudrate)
            {
                _dev->Begin(CRSF_TX_BAUDRATE_FAST);
                DEBUG_PRINTLN("Switch to 400000 baud");
            }
            else
            {
                _dev->Begin(CRSF_TX_BAUDRATE_SLOW);
                DEBUG_PRINTLN("Switch to 115000 baud");
            }
            p_slowBaudrate = !p_slowBaudrate;
        }

        p_UartNextCheck = now;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
}
