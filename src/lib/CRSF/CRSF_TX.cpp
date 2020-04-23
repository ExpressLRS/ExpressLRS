#include "CRSF_TX.h"
#include "debug.h"
#include "FIFO.h"
//#include "common.h"
#include <string.h>

extern void platform_wd_feed(void);

void paramNullCallback(uint8_t const *, uint16_t){};
void (*CRSF_TX::ParamWriteCallback)(uint8_t const *msg, uint16_t len) = &paramNullCallback;

///Out FIFO to buffer messages///
FIFO SerialOutFIFO;

void CRSF_TX::Begin(void)
{
    CRSF::Begin();
    p_UartNextCheck = millis() + UARTwdtInterval * 2;
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
    //uint8_t outBuffer[len] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // this is nok with volatile
    memcpy(&outBuffer[3], (void *)&LinkStatistics, LinkStatisticsFrameLength);

    CrsfFramePushToFifo(outBuffer, len);
}

void CRSF_TX::sendSetVTXchannelToRadio(uint8_t band, uint8_t channel)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(VTXcontrolFrameLength);
    //uint8_t outBuffer[len] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = CRSF_FRAME_SIZE(VTXcontrolFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_COMMAND;
    outBuffer[3] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_RECEIVER;
    /////////////////////////////////////////////
    outBuffer[5] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[6] = CRSF_ADDRESS_CRSF_RECEIVER;
    outBuffer[7] = 0x08; // vtx command
    ////////////////////////////////////////////
    outBuffer[8] = channel + 8 * band;
    outBuffer[9] = 0x00;
    outBuffer[10] = 0x00;
    outBuffer[11] = 0x00;
    outBuffer[12] = 0x00;
    outBuffer[13] = 0x00;

    CrsfFramePushToFifo(outBuffer, len);
}

void CRSF_TX::sendLUAresponseToRadio(uint8_t *data, uint8_t size)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(2 + size);
    //uint8_t outBuffer[len] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(2 + size);
    outBuffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    for (uint8_t i = 0; i < size; i++)
        outBuffer[5 + i] = data[i];

    CrsfFramePushToFifo(outBuffer, len);
}

void CRSF_TX::BatterySensorSend(void)
{
    if (!CRSFstate)
        return;

    uint8_t len = CRSF_EXT_FRAME_SIZE(BattSensorFrameLength);
    //uint8_t outBuffer[len] = {0};

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
}

#if (FEATURE_OPENTX_SYNC)
void CRSF_TX::sendSyncPacketToRadio() // in values in us.
{
    if (RCdataLastRecv && CRSFstate)
    {
        uint32_t current = millis();
        //DEBUG_PRINT(OpenTXsyncOffset);

        if (current >= OpenTXsynNextSend)
        {
            uint32_t packetRate = RequestedRCpacketInterval;
            packetRate *= 10; //convert from us to right format

            int32_t offset = OpenTXsyncOffset - RequestedRCpacketAdvance;
            offset *= 10;

            uint8_t len = CRSF_EXT_FRAME_SIZE(OpenTXsyncFrameLength);
            //uint8_t outBuffer[len] = {0};

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

            OpenTXsynNextSend = current + OpenTXsyncPakcetInterval;
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
        OpenTXsynNextSend = millis() + 60;
#endif
        DEBUG_PRINTLN("CRSF UART Connected");
        connected();
    }

    switch (*input++)
    {
        case CRSF_FRAMETYPE_PARAMETER_WRITE:
        {
            if (input[0] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
                input[1] == CRSF_ADDRESS_RADIO_TRANSMITTER)
            {
                ParamWriteCallback(&input[2], 2);
            }
        }
        case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
        {
            //DEBUG_PRINT("X");
#if (FEATURE_OPENTX_SYNC)
            RCdataLastRecv = micros();
#endif
            (RCdataCallback1)((crsf_channels_t *)input); // run new RC data callback
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
    if (now > p_UartNextCheck)
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

        p_UartNextCheck = now + UARTwdtInterval;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
}
