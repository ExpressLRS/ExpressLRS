#include "CRSF_TX.h"
#include "../../src/utils.h"
#include "../../src/debug.h"

#include "FIFO.h"

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

void ICACHE_RAM_ATTR CRSF_TX::sendLinkStatisticsToRadio()
{
    if (!CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // this is nok with volatile
    memcpy(&outBuffer[3], (void *)&LinkStatistics, LinkStatisticsFrameLength);

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
}

void ICACHE_RAM_ATTR CRSF_TX::sendSetVTXchannelToRadio(uint8_t band, uint8_t channel)
{
    if (!CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(VTXcontrolFrameLength)] = {0};

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

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
}

void ICACHE_RAM_ATTR CRSF_TX::sendLUAresponseToRadio(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4)
{
    if (!CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(LUArespLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(LUArespLength);
    outBuffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    outBuffer[5] = val1;
    outBuffer[6] = val2;
    outBuffer[7] = val3;
    outBuffer[8] = val4;

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
}

void ICACHE_RAM_ATTR CRSF_TX::sendLinkBattSensorToRadio()
{
    if (!CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(BattSensorFrameLength)] = {0};

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

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
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

            uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(OpenTXsyncFrameLength)] = {0};

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

            CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));

            OpenTXsynNextSend = current + OpenTXsyncPakcetInterval;
        }
    }
}
#endif /* FEATURE_OPENTX_SYNC */

void ICACHE_RAM_ATTR CRSF_TX::processPacket(uint8_t const *input)
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
        //DEBUG_PRINTLN("Got Other Packet");
        DEBUG_PRINTLN("L");
        if (input[0] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
            input[1] == CRSF_ADDRESS_RADIO_TRANSMITTER)
        {
            ParameterUpdateData[0] = input[2];
            ParameterUpdateData[1] = input[3];
            RecvParameterUpdate();
        }
    }
    case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
    {
        DEBUG_PRINT("X");
#if (FEATURE_OPENTX_SYNC)
        RCdataLastRecv = micros();
#endif
        StoreChannelData(input);
        (RCdataCallback1)(); // run new RC data callback
        break;
    }
    default:
        break;
    };
}

void CRSF_TX::handleUartIn(void) // Merge with RX version...
{
    uint8_t split_cnt = 0;

#if (FEATURE_OPENTX_SYNC)
    sendSyncPacketToRadio();
#endif

    while (_dev->available() && ((++split_cnt & 0xF) > 0))
    {
        uint8_t *ptr = HandleUartIn(_dev->read());
        if (ptr)
        {
            processPacket(ptr);
            /* Can write right after successful package reception */
            _dev->write(SerialOutFIFO);
        }
    }

    uart_wdt();
}

void CRSF_TX::uart_wdt(void)
{
    uint32_t now = millis();
    if (now > p_UartNextCheck)
    {
        DEBUG_PRINT("UART RX packets! Bad: ");
        DEBUG_PRINT(BadPktsCount);
        DEBUG_PRINT(" Good: ");
        DEBUG_PRINTLN(GoodPktsCount);

        if (BadPktsCount >= GoodPktsCount)
        {
            DEBUG_PRINTLN("  Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                SerialOutFIFO.flush();
                DEBUG_PRINTLN("UART WDT: Disconnected");
                disconnected();
                CRSFstate = false;
#if (FEATURE_OPENTX_SYNC)
                OpenTXsynNextSend = 0;
                OpenTXsyncOffset = 0;
#endif
            }

            _dev->end();
            if (p_slowBaudrate)
            {
                _dev->Begin(CRSF_OPENTX_BAUDRATE);
                DEBUG_PRINTLN("UART WDT: Switch to 400000 baud");
            }
            else
            {
                _dev->Begin(CRSF_OPENTX_SLOW_BAUDRATE);
                DEBUG_PRINTLN("UART WDT: Switch to 115000 baud");
            }
            p_slowBaudrate = !p_slowBaudrate;
        }

        p_UartNextCheck = now + UARTwdtInterval;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
}

void CRSF_TX::StoreChannelData(uint8_t const *const data)
{
    uint8_t switch_state;

    // data is packed as 11 bits per channel
    const crsf_channels_t *const rcChannels = (crsf_channels_t *)data;

    // TODO: loop N_CHANNELS times
    ChannelDataIn[0] = (rcChannels->ch0);
    ChannelDataIn[1] = (rcChannels->ch1);
    ChannelDataIn[2] = (rcChannels->ch2);
    ChannelDataIn[3] = (rcChannels->ch3);
    ChannelDataIn[4] = (rcChannels->ch4);
    ChannelDataIn[5] = (rcChannels->ch5);
    ChannelDataIn[6] = (rcChannels->ch6);
    ChannelDataIn[7] = (rcChannels->ch7);
    ChannelDataIn[8] = (rcChannels->ch8);
    ChannelDataIn[9] = (rcChannels->ch9);
    ChannelDataIn[10] = (rcChannels->ch10);
    ChannelDataIn[11] = (rcChannels->ch11);
    ChannelDataIn[12] = (rcChannels->ch12);
    ChannelDataIn[13] = (rcChannels->ch13);
    ChannelDataIn[14] = (rcChannels->ch14);
    ChannelDataIn[15] = (rcChannels->ch15);

    p_auxChannelsChanged = 0;

    /**
     * Convert the rc data corresponding to switches to 2 bit values.
     *
     * I'm defining channels 4 through 11 inclusive as representing switches
     * Take the input values and convert them to the range 0 - 2.
     * (not 0-3 because most people use 3 way switches and expect the middle
     *  position to be represented by a middle numeric value)
     */
    for (int i = 0; i < N_SWITCHES; i++)
    {
        // input is 0 - 2048, output is 0 - 2
        switch_state = ChannelDataIn[i + N_CONTROLS] / 682;
        // Check if state is changed
        if (switch_state != currentSwitches[i])
            p_auxChannelsChanged |= (0x1 << i);
        currentSwitches[i] = switch_state;
    }
}

// **********************************************************************************
//
// TODO: Move switch/aux code to generic library!
//
// **********************************************************************************

/**
 * Determine which switch to send next.
 * If any switch has changed since last sent, we send the lowest index changed switch
 * and set nextSwitchIndex to that value + 1.
 * If no switches have changed then we send nextSwitchIndex and increment the value.
 * For pure sequential switches, all 8 switches are part of the round-robin sequence.
 * For hybrid switches, switch 0 is sent with every packet and the rest of the switches
 * are in the round-robin.
 */
uint8_t ICACHE_RAM_ATTR CRSF_TX::getNextSwitchIndex()
{
    int i;
    int firstSwitch = 0; // sequential switches includes switch 0

#ifdef HYBRID_SWITCHES_8
    firstSwitch = 1; // skip 0 since it is sent on every packet
#endif

    // look for a changed switch
    for (i = firstSwitch; i < N_SWITCHES; i++)
    {
        if (currentSwitches[i] != sentSwitches[i])
            break;
    }
    // if we didn't find a changed switch, we get here with i==N_SWITCHES
    if (i == N_SWITCHES)
        i = nextSwitchIndex;

    // keep track of which switch to send next if there are no changed switches
    // during the next call.
    nextSwitchIndex = (i + 1) % N_SWITCHES;

#ifdef HYBRID_SWITCHES_8
    // for hydrid switches 0 is sent on every packet, so we can skip
    // that value for the round-robin
    if (nextSwitchIndex == 0)
        nextSwitchIndex = 1;
#endif

    // since we are going to send i, we can put that value into the sent list.
    sentSwitches[i] = currentSwitches[i];

    return i;
}
