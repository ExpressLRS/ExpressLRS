#include <Arduino.h>
#include "../../src/targets.h"
#include "CRSF.h"
#include "../../lib/FIFO/FIFO.h"
#include "../../src/debug.h"

HwSerial *CRSF::_dev = NULL;

///Out FIFO to buffer messages///
FIFO SerialOutFIFO;

//U1RXD_IN_IDX
//U1TXD_OUT_IDX

volatile bool CRSF::ignoreSerialData = false;
volatile bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

void inline CRSF::nullCallback(void){};

void (*CRSF::RCdataCallback1)() = &nullCallback; // function is called whenever there is new RC data.
void (*CRSF::RCdataCallback2)() = &nullCallback; // function is called whenever there is new RC data.

void (*CRSF::disconnected)() = &nullCallback; // called when CRSF stream is lost
void (*CRSF::connected)() = &nullCallback;    // called when CRSF stream is regained

void (*CRSF::RecvParameterUpdate)() = &nullCallback; // called when recv parameter update req, ie from LUA

/// UART Handling ///
bool CRSF::CRSFstate = false;
bool CRSF::IsUARTslowBaudrate = false;

uint32_t CRSF::UARTwdtNextCheck = 0;
#define UARTwdtInterval 1000 // for the UART wdt, every 1000ms we change bauds when connect is lost

uint32_t CRSF::GoodPktsCount = 0;
uint32_t CRSF::BadPktsCount = 0;

volatile uint8_t CRSF::SerialInPacketLen = 0;                     // length of the CRSF packet as measured
volatile uint8_t CRSF::SerialInPacketPtr = 0;                     // index where we are reading/writing
volatile uint8_t CRSF::SerialInBuffer[CRSF_MAX_PACKET_LEN] = {0}; // max 64 bytes for CRSF packet
volatile uint16_t CRSF::ChannelDataIn[16] = {0};
volatile uint16_t CRSF::ChannelDataInPrev[16] = {0};

// current and sent switch values, used for prioritising sequential switch transmission
uint8_t CRSF::currentSwitches[N_SWITCHES] = {0};
uint8_t CRSF::sentSwitches[N_SWITCHES] = {0};

uint8_t CRSF::nextSwitchIndex = 0; // for round-robin sequential switches

volatile uint8_t CRSF::ParameterUpdateData[2] = {0};

volatile crsf_channels_s CRSF::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;
volatile crsf_sensor_battery_s CRSF::TLMbattSensor;

#if defined(PLATFORM_ESP32) || defined(TARGET_R9M_TX)
#if defined(FEATURE_OPENTX_SYNC)
volatile uint32_t CRSF::RCdataLastRecv = 0;
volatile int32_t CRSF::OpenTXsyncOffset = 0;
volatile uint32_t CRSF::OpenTXsynNextSend = 0;
volatile uint32_t CRSF::RequestedRCpacketInterval = 10000; // default to 250hz as per 'normal'
#endif
#endif

void CRSF::Begin()
{
    //DEBUG_PRINTLN("About to start CRSF task...");

    //The master module requires that the serial communication is bidirectional
    //The Reciever uses seperate rx and tx pins

    DEBUG_PRINTLN("CRSF UART LISTEN TASK STARTED");
    uint32_t now = millis();
    UARTwdtNextCheck = now + UARTwdtInterval * 2;
}

#if defined(PLATFORM_ESP32) || defined(TARGET_R9M_TX)
/**
 * Determine which switch to send next.
 * If any switch has changed since last sent, we send the lowest index changed switch
 * and set nextSwitchIndex to that value + 1.
 * If no switches have changed then we send nextSwitchIndex and increment the value.
 * For pure sequential switches, all 8 switches are part of the round-robin sequence.
 * For hybrid switches, switch 0 is sent with every packet and the rest of the switches
 * are in the round-robin.
 */
uint8_t ICACHE_RAM_ATTR CRSF::getNextSwitchIndex()
{
    int i;
    int firstSwitch = 0; // sequential switches includes switch 0

#if defined HYBRID_SWITCHES_8
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
    nextSwitchIndex = (i + 1) % 8;

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

void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = CalcCRC(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
        SerialOutFIFO.push(LinkStatisticsFrameLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, LinkStatisticsFrameLength + 4);
    }
}

void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel)
{
    // this is an 'extended header frame'

    uint8_t outBuffer[VTXcontrolFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = VTXcontrolFrameLength + 2;
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

    uint8_t crc1 = CalcCRCcmd(&outBuffer[2], VTXcontrolFrameLength + 1);

    outBuffer[14] = crc1;

    uint8_t crc2 = CalcCRC(&outBuffer[2], VTXcontrolFrameLength + 2);

    outBuffer[15] = crc2;

    if (CRSF::CRSFstate)
    {
        SerialOutFIFO.push(VTXcontrolFrameLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, VTXcontrolFrameLength + 4);
    }
}

void ICACHE_RAM_ATTR CRSF::sendLUAresponse(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4)
{
#define LUArespLength 6

    uint8_t outBuffer[LUArespLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LUArespLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    outBuffer[5] = val1;
    outBuffer[6] = val2;
    outBuffer[7] = val3;
    outBuffer[8] = val4;

    uint8_t crc = CalcCRC(&outBuffer[2], LUArespLength + 1);

    outBuffer[LUArespLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
        SerialOutFIFO.push(LUArespLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, LUArespLength + 4);
    }
}

void ICACHE_RAM_ATTR CRSF::sendLinkBattSensorToTX()
{
    uint8_t outBuffer[BattSensorFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = BattSensorFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

    //memcpy(outBuffer + 3, (byte *)&TLMbattSensor, BattSensorFrameLength);
    outBuffer[3] = CRSF::TLMbattSensor.voltage << 8;
    outBuffer[4] = CRSF::TLMbattSensor.voltage;
    outBuffer[5] = CRSF::TLMbattSensor.current << 8;
    outBuffer[6] = CRSF::TLMbattSensor.current;
    outBuffer[7] = CRSF::TLMbattSensor.capacity << 16;
    outBuffer[9] = CRSF::TLMbattSensor.capacity << 8;
    outBuffer[10] = CRSF::TLMbattSensor.capacity;
    outBuffer[11] = CRSF::TLMbattSensor.remaining;

    uint8_t crc = CalcCRC(&outBuffer[2], BattSensorFrameLength + 1);

    outBuffer[BattSensorFrameLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
        SerialOutFIFO.push(BattSensorFrameLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, BattSensorFrameLength + 4);
    }
    //DEBUG_PRINTLN(CRSFoutBuffer[0]);
}

#if defined(FEATURE_OPENTX_SYNC)
void ICACHE_RAM_ATTR CRSF::UpdateOpenTxSyncOffset()
{
    CRSF::OpenTXsyncOffset = micros() - CRSF::RCdataLastRecv;
    //DEBUG_PRINTLN(CRSF::OpenTXsyncOffset);
}

void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX() // in values in us.
{
    uint32_t current = millis();
    if (current > OpenTXsynNextSend)
    {
        //DEBUG_PRINTLN(CRSF::OpenTXsyncOffset);

        if (CRSF::CRSFstate)
        {
            uint32_t packetRate = CRSF::RequestedRCpacketInterval * 10; //convert from us to right format
            int32_t offset = CRSF::OpenTXsyncOffset * 10 - 8000;        // + offset that seems to be needed
            uint8_t outBuffer[OpenTXsyncFrameLength + 4] = {0};

            outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER; //0xEA
            outBuffer[1] = OpenTXsyncFrameLength + 2;      // equals 13?
            outBuffer[2] = CRSF_FRAMETYPE_RADIO_ID;        // 0x3A

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

            uint8_t crc = CalcCRC(&outBuffer[2], OpenTXsyncFrameLength + 1);

            outBuffer[OpenTXsyncFrameLength + 3] = crc;

            SerialOutFIFO.push(OpenTXsyncFrameLength + 4); // length
            SerialOutFIFO.pushBytes(outBuffer, OpenTXsyncFrameLength + 4);
        }
        OpenTXsynNextSend = current + OpenTXsyncPakcetInterval;
    }
}
#endif

bool ICACHE_RAM_ATTR CRSF::TX_ProcessPacket()
{
    bool ret = false;
    if (CRSFstate == false)
    {
        CRSFstate = true;
        DEBUG_PRINTLN("CRSF UART Connected");
        connected();
#ifdef GPIO_PIN_LED_RED
        digitalWrite(GPIO_PIN_LED_RED, LOW);
#endif
    }

    if (CRSF::SerialInBuffer[2] == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        DEBUG_PRINTLN("Got Other Packet");
        if (SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
            SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER)
        {
            ParameterUpdateData[0] = SerialInBuffer[5];
            ParameterUpdateData[1] = SerialInBuffer[6];
            RecvParameterUpdate();
            ret = true;
        }
    }
    else if (CRSF::SerialInBuffer[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
#if defined(FEATURE_OPENTX_SYNC)
        CRSF::RCdataLastRecv = micros();
#endif
        GetChannelDataIn();
        (RCdataCallback1)(); // run new RC data callback
        (RCdataCallback2)(); // run new RC data callback
        ret = true;
    }
    return ret;
}

void ICACHE_RAM_ATTR CRSF::TX_handleUartIn(void) // Merge with RX version...
{
    uint8_t inChar, split_cnt = 0;

#if defined(FEATURE_OPENTX_SYNC)
    sendSyncPacketToTX();
#endif
    wdtUART();

    while (_dev->available() && ((++split_cnt & 0xF) > 0))
    {
        inChar = _dev->read();

        if (SerialInPacketPtr >= CRSF_MAX_PACKET_LEN)
        {
            // we reached the maximum allowable packet length, so start again because shit fucked up hey.
            SerialInPacketPtr = 0;
        }

        // store byte
        SerialInBuffer[SerialInPacketPtr++] = inChar;

        if (CRSFframeActive == false)
        {
            if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER || inChar == CRSF_SYNC_BYTE)
            {
                CRSFframeActive = true;
                SerialInPacketLen = 0;
            }
            else
            {
                SerialInPacketPtr = 0;
            }
        }
        else
        {
            if (SerialInPacketLen == 0) // we read the packet length and save it
            {
                SerialInPacketLen = inChar;
                if (SerialInPacketLen < 3 || ((CRSF_MAX_PACKET_LEN - 2) < SerialInPacketLen))
                {
                    // failure -> discard
                    CRSFframeActive = false;
                    SerialInPacketPtr = 0;
                    SerialInPacketLen = 0;
                    BadPktsCount++;
                }
            }
            else
            {
                if (SerialInPacketPtr == (SerialInPacketLen + 2))
                {
                    uint8_t CalculatedCRC = CalcCRC((uint8_t *)&SerialInBuffer[2], SerialInPacketLen - 1);

                    if (CalculatedCRC == inChar)
                    {
                        if (TX_ProcessPacket())
                        {
                            _dev->write(SerialOutFIFO);
                        }

                        GoodPktsCount++;
                    }
                    else
                    {
                        DEBUG_PRINTLN("UART CRC failure");
                        // DEBUG_PRINTLN(SerialInPacketPtr, HEX);
                        // DEBUG_PRINT("Expected: ");
                        // DEBUG_PRINTLN(CalculatedCRC, HEX);
                        // DEBUG_PRINT("Got: ");
                        // DEBUG_PRINTLN(inChar, HEX);
                        for (int i = 0; i < (SerialInPacketLen + 2); i++)
                        {
                            DEBUG_PRINT(SerialInBuffer[i], HEX);
                            DEBUG_PRINT(" ");
                        }
                        DEBUG_PRINTLN();
                        //DEBUG_PRINTLN();
                        BadPktsCount++;
                    }
                    // packet handled, start next
                    CRSFframeActive = false;
                    SerialInPacketPtr = 0;
                    SerialInPacketLen = 0;
                }
            }
        }
    }
}

void ICACHE_RAM_ATTR CRSF::wdtUART()
{
    uint32_t now = millis();
    if (now > UARTwdtNextCheck)
    {
        DEBUG_PRINT("UART RX packets! Bad: ");
        DEBUG_PRINT(BadPktsCount);
        DEBUG_PRINT(" Good: ");
        DEBUG_PRINTLN(GoodPktsCount);

        if (BadPktsCount >= GoodPktsCount)
        {
#ifdef GPIO_PIN_LED_RED
            digitalWrite(GPIO_PIN_LED_RED, HIGH);
#endif
            DEBUG_PRINTLN("  Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                SerialOutFIFO.flush();
                DEBUG_PRINTLN("UART WDT: Disconnected");
                disconnected();
                CRSFstate = false;
            }
            if (IsUARTslowBaudrate)
            {
                _dev->Begin(CRSF_OPENTX_BAUDRATE);
                DEBUG_PRINTLN("UART WDT: Switch to 400000 baud");
            }
            else
            {
                _dev->Begin(CRSF_OPENTX_SLOW_BAUDRATE);
                DEBUG_PRINTLN("UART WDT: Switch to 115000 baud");
            }
            IsUARTslowBaudrate = !IsUARTslowBaudrate;
        }

        UARTwdtNextCheck = now + UARTwdtInterval;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
}

/**
 * Convert the rc data corresponding to switches to 2 bit values.
 *
 * I'm defining channels 4 through 11 inclusive as representing switches
 * Take the input values and convert them to the range 0 - 2.
 * (not 0-3 because most people use 3 way switches and expect the middle
 *  position to be represented by a middle numeric value)
 */
void ICACHE_RAM_ATTR CRSF::updateSwitchValues()
{
    for (int i = 0; i < N_SWITCHES; i++)
    {
        currentSwitches[i] = ChannelDataIn[i + 4] / 682; // input is 0 - 2048, output is 0 - 2
    }
}

void ICACHE_RAM_ATTR CRSF::GetChannelDataIn() // data is packed as 11 bits per channel
{
#define SERIAL_PACKET_OFFSET 3

    memcpy((uint16_t *)ChannelDataInPrev, (uint16_t *)ChannelDataIn, 16); //before we write the new RC channel data copy the old data

    const crsf_channels_t *const rcChannels =
        (crsf_channels_t *)&CRSF::SerialInBuffer[SERIAL_PACKET_OFFSET];
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

    updateSwitchValues();
}

#endif // TX

#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX)
void CRSF::sendLinkStatisticsToFC()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = CalcCRC(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

    _dev->write(outBuffer, LinkStatisticsFrameLength + 4);
}

void ICACHE_RAM_ATTR CRSF::sendRCFrameToFC()
{
    uint8_t outBuffer[RCframeLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = RCframeLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memcpy(outBuffer + 3, (byte *)&PackedRCdataOut, RCframeLength);

    uint8_t crc = CalcCRC(&outBuffer[2], RCframeLength + 1);

    outBuffer[RCframeLength + 3] = crc;

    _dev->write(outBuffer, RCframeLength + 4);
    //DEBUG_PRINT(".");
}

void CRSF::RX_processPacket()
{
    if (SerialInBuffer[2] == CRSF_FRAMETYPE_COMMAND)
    {
        //DEBUG_PRINTLN("Got CMD Packet");
        if (SerialInBuffer[3] == 0x62 && SerialInBuffer[4] == 0x6c)
        {
            delay(100);
            DEBUG_PRINTLN("Jumping to Bootloader...");
            delay(100);
#ifdef PLATFORM_STM32
            HAL_NVIC_SystemReset();
#endif
#ifdef PLATFORM_ESP8266
            ESP.restart();
#endif
        }
    }
    else if (SerialInBuffer[2] == CRSF_FRAMETYPE_BATTERY_SENSOR)
    {
        TLMbattSensor.voltage = (SerialInBuffer[3] << 8) + SerialInBuffer[4];
        TLMbattSensor.current = (SerialInBuffer[5] << 8) + SerialInBuffer[6];
        TLMbattSensor.capacity = (SerialInBuffer[7] << 16) + (SerialInBuffer[8] << 8) + SerialInBuffer[9];
        TLMbattSensor.remaining = SerialInBuffer[9];
    }
}

void CRSF::RX_handleUartIn(void)
{
    uint8_t inChar, split_cnt = 0;

    while (_dev->available() && ((++split_cnt & 0xF) > 0))
    {
        inChar = _dev->read();

        if (SerialInPacketPtr >= CRSF_MAX_PACKET_LEN)
        {
            // we reached the maximum allowable packet length, so start again because shit fucked up hey.
            SerialInPacketPtr = 0;
        }

        // store byte
        SerialInBuffer[SerialInPacketPtr++] = inChar;

        if (CRSFframeActive == false)
        {
            if (inChar == CRSF_ADDRESS_CRSF_RECEIVER || inChar == CRSF_SYNC_BYTE)
            {
                CRSFframeActive = true;
                SerialInPacketLen = 0;
            }
            else
            {
                SerialInPacketPtr = 0;
            }
        }
        else
        {
            if (SerialInPacketLen == 0) // we read the packet length and save it
            {
                SerialInPacketLen = inChar;
                if (SerialInPacketLen < 3 || ((CRSF_MAX_PACKET_LEN - 2) < SerialInPacketLen))
                {
                    // failure -> discard
                    CRSFframeActive = false;
                    SerialInPacketPtr = 0;
                    SerialInPacketLen = 0;
                    BadPktsCount++;
                }
            }
            else
            {
                if (SerialInPacketPtr == (SerialInPacketLen + 2))
                {
                    uint8_t CalculatedCRC = CalcCRC((uint8_t *)&SerialInBuffer[2], SerialInPacketLen - 1);

                    if (CalculatedCRC == inChar)
                    {
                        RX_processPacket();
                        GoodPktsCount++;
                    }
                    else
                    {
                        //DEBUG_PRINTLN("UART CRC failure");
                        //DEBUG_PRINTLN(SerialInPacketPtr, HEX);
                        //DEBUG_PRINT("Expected: ");
                        //DEBUG_PRINTLN(CalculatedCRC, HEX);
                        //DEBUG_PRINT("Got: ");
                        //DEBUG_PRINTLN(inChar, HEX);
                        //for (int i = 0; i < SerialInPacketPtr + 2; i++)
                        // {
                        //DEBUG_PRINT(SerialInBuffer[i], HEX);
                        //    DEBUG_PRINT(" ");
                        //}
                        //DEBUG_PRINTLN();
                        //DEBUG_PRINTLN();
                        BadPktsCount++;
                    }

                    // packet handled, start next
                    CRSFframeActive = false;
                    SerialInPacketPtr = 0;
                    SerialInPacketLen = 0;
                }
            }
        }
    }
}
#endif // RX

void CRSF::process_input(void)
{
    while (SerialInPacketPtr < CRSF_MAX_PACKET_LEN && _dev->available())
    {
        SerialInBuffer[SerialInPacketPtr++] = _dev->read();
    }
    command_find_and_dispatch();
}

void CRSF::command_find_and_dispatch(void)
{
    volatile uint8_t *ptr = SerialInBuffer;
    uint8_t count = SerialInPacketPtr;
    if (count < 3)
        return;

    while (--count)
    {
        uint8_t cmd = *ptr++;

        if (cmd == CRSF_ADDRESS_CRSF_RECEIVER ||
            cmd == CRSF_ADDRESS_CRSF_TRANSMITTER ||
            cmd == CRSF_SYNC_BYTE)
        {
            uint8_t len = *ptr++;
            if (len < 3 || ((CRSF_MAX_PACKET_LEN - 2) < len))
            {
                printf("  invalid len %u for cmd: %X!\n", len, cmd);
            }
            else if (len <= count)
            {
                len -= 1; // remove CRC
                // enough data, check crc...
                uint8_t crc = CalcCRC(ptr, (len));
                if (crc == ptr[len])
                {
                    // CRC ok, dispatch command
                    //printf("  CRC OK\n");
#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX)
                    RX_processPacket();
#else
                    TX_ProcessPacket();
#endif
                }
                else
                {
                    // Bad CRC...
                    //printf("  CRC BAD\n");
                }
                // pass handled command
                ptr += (len);
                uint8_t end = ptr - SerialInBuffer + 1;
                int needcopy = (int)SerialInPacketPtr - (end);
                if (0 < needcopy)
                {
                    memmove((void *)&SerialInBuffer[0], (void *)&SerialInBuffer[end], needcopy);
                }
                SerialInPacketPtr = count = needcopy;
                ptr = SerialInBuffer;
            }
            else
            {
                // need more data...
                goto exit_dispatch;
            }
        }
    }

    SerialInPacketPtr = 0; // just grab, discard all

exit_dispatch:
    return;
}
