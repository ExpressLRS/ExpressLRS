#include "CRSF_TX.h"
#include "debug.h"
#include "FIFO.h"
#include <string.h>

void paramNullCallback(uint8_t const *, uint16_t){};
void (*CRSF_TX::ParamWriteCallback)(uint8_t const *msg, uint16_t len) = &paramNullCallback;

///Out FIFO to buffer messages///
//FIFO SerialOutFIFO;

enum {
    SEND_NA = 0,
    SEND_LNK_STAT = 1 << 0,
    SEND_BATT = 1 << 1,
    SEND_LUA = 1 << 2,
    SEND_MSP = 1 << 3,
    SEND_SYNC = 1 << 4,
};
static volatile uint_fast8_t DMA_ATTR send_buffers = SEND_NA;

void CRSF_TX::Begin(void)
{
    CRSF::Begin();
    p_UartNextCheck = millis(); //  +UARTwdtInterval * 2;
}

void ICACHE_RAM_ATTR CRSF_TX::CrsfFramePushToFifo(uint8_t *buff, uint8_t size)
{
    buff[size - 1] = CalcCRC(&buff[2], (buff[1] - 1));
    //SerialOutFIFO.push(size); // length
    //SerialOutFIFO.pushBytes(buff, size);
    _dev->write(buff, size);
}

void CRSF_TX::LinkStatisticsSend(void)
{
    send_buffers |= SEND_LNK_STAT;
}
void CRSF_TX::LinkStatisticsProcess(void)
{
    if (!(send_buffers & SEND_LNK_STAT))
        return;
    send_buffers &= ~SEND_LNK_STAT;

    uint8_t len = CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength);

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // this is nok with volatile
    memcpy(&outBuffer[3], (void *)&LinkStatistics, LinkStatisticsFrameLength);

    CrsfFramePushToFifo(outBuffer, len);
    platform_wd_feed();
}

void CRSF_TX::sendLUAresponseToRadio(uint8_t *data, uint8_t size)
{
    memcpy(lua_buff, data, size);
    send_buffers |= SEND_LUA;
}

void CRSF_TX::LuaResponseProcess(void)
{
    if (!(send_buffers & SEND_LUA))
        return;
    send_buffers &= ~SEND_LUA;

    uint8_t len = CRSF_EXT_FRAME_SIZE(2 + sizeof(lua_buff));

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(2 + sizeof(lua_buff));
    outBuffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    for (uint8_t i = 0; i < sizeof(lua_buff); i++)
        outBuffer[5 + i] = lua_buff[i];

    CrsfFramePushToFifo(outBuffer, len);
    platform_wd_feed();
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
    platform_wd_feed();
}

void CRSF_TX::BatterySensorSend(void)
{
    send_buffers |= SEND_BATT;
}
void CRSF_TX::BatteryStatisticsProcess(void)
{
    if (!(send_buffers & SEND_BATT))
        return;
    send_buffers &= ~SEND_BATT;

    uint8_t len = CRSF_EXT_FRAME_SIZE(BattSensorFrameLength);

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(BattSensorFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

    outBuffer[3] = (TLMbattSensor.voltage >> 8) & 0xff;
    outBuffer[4] = TLMbattSensor.voltage & 0xff;
    outBuffer[5] = (TLMbattSensor.current >> 8) & 0xff;
    outBuffer[6] = TLMbattSensor.current & 0xff;
    outBuffer[7] = (TLMbattSensor.capacity >> 16) & 0xff;
    outBuffer[9] = (TLMbattSensor.capacity >> 8) & 0xff;
    outBuffer[10] = TLMbattSensor.capacity & 0xff;
    outBuffer[11] = TLMbattSensor.remaining;

    CrsfFramePushToFifo(outBuffer, len);
    platform_wd_feed();
}

#if (FEATURE_OPENTX_SYNC)
void CRSF_TX::sendSyncPacketToRadio() // in values in us.
{
    if (RCdataLastRecv && CRSFstate)
    {
        uint32_t current = millis();

        if (OpenTXsyncPakcetInterval <= (current - OpenTXsynNextSend))
        {
            OpenTXsynNextSend = current;
            platform_wd_feed();

            uint32_t packetRate = RequestedRCpacketInterval;
            packetRate *= 10; //convert from us to right format

            int32_t offset = OpenTXsyncOffset - RequestedRCpacketAdvance;
            offset *= 10;

            uint8_t len = CRSF_EXT_FRAME_SIZE(OpenTXsyncFrameLength);

            outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;         // 0xEA
            outBuffer[1] = CRSF_FRAME_SIZE(OpenTXsyncFrameLength); // 13
            outBuffer[2] = CRSF_FRAMETYPE_RADIO_ID;                // 0x3A

            outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER; //0XEA
            outBuffer[4] = 0x00;                           //??? not sure doesn't seem to matter
            outBuffer[5] = CRSF_FRAMETYPE_OPENTX_SYNC;     //0X10

            outBuffer[6] = (packetRate & 0xFF000000) >> 24;
            outBuffer[7] = (packetRate & 0x00FF0000) >> 16;
            outBuffer[8] = (packetRate & 0x0000FF00) >> 8;
            outBuffer[9] = (packetRate & 0x000000FF) >> 0;

            outBuffer[10] = (offset & 0xFF000000) >> 24;
            outBuffer[11] = (offset & 0x00FF0000) >> 16;
            outBuffer[12] = (offset & 0x0000FF00) >> 8;
            outBuffer[13] = (offset & 0x000000FF) >> 0;

            CrsfFramePushToFifo(outBuffer, len);
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

uint8_t CRSF_TX::handleUartIn(volatile uint8_t &rx_data_rcvd) // Merge with RX version...
{
    uint8_t can_send = 0;
    uint8_t split_cnt = 0;

    while (rx_data_rcvd == 0 && _dev->available() && ((++split_cnt & 0x7) > 0))
    {
        uint8_t *ptr = HandleUartIn(_dev->read());
        if (ptr)
        {
            processPacket(ptr);
            platform_wd_feed();

            if (rx_data_rcvd == 0)
            {
                /* Can write right after successful package reception */
                //_dev->write(SerialOutFIFO);
#if (FEATURE_OPENTX_SYNC)
                sendSyncPacketToRadio();
#endif
                LuaResponseProcess();
                LinkStatisticsProcess();
                BatteryStatisticsProcess();
                can_send = 1;
            }
        }
    }

    if (rx_data_rcvd == 0)
        uart_wdt();

    return can_send;
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
                //SerialOutFIFO.flush();
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
