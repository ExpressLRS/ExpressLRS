#include "CRSF.h"
#include "FIFO.h"

#include "../../src/utils.h"
#include "../../src/debug.h"

/* CRC8 implementation with polynom = x​7​+ x​6​+ x​4​+ x​2​+ x​0 ​(0xD5) */
const unsigned char crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9};

#if (CRSF_CMD_CRC)
const unsigned char crc8tabcmd[256] =
    {0x00, 0xBA, 0xCE, 0x74, 0x26, 0x9C, 0xE8, 0x52, 0x4C, 0xF6, 0x82, 0x38, 0x6A, 0xD0, 0xA4, 0x1E,
     0x98, 0x22, 0x56, 0xEC, 0xBE, 0x04, 0x70, 0xCA, 0xD4, 0x6E, 0x1A, 0xA0, 0xF2, 0x48, 0x3C, 0x86,
     0x8A, 0x30, 0x44, 0xFE, 0xAC, 0x16, 0x62, 0xD8, 0xC6, 0x7C, 0x08, 0xB2, 0xE0, 0x5A, 0x2E, 0x94,
     0x12, 0xA8, 0xDC, 0x66, 0x34, 0x8E, 0xFA, 0x40, 0x5E, 0xE4, 0x90, 0x2A, 0x78, 0xC2, 0xB6, 0x0C,
     0xAE, 0x14, 0x60, 0xDA, 0x88, 0x32, 0x46, 0xFC, 0xE2, 0x58, 0x2C, 0x96, 0xC4, 0x7E, 0x0A, 0xB0,
     0x36, 0x8C, 0xF8, 0x42, 0x10, 0xAA, 0xDE, 0x64, 0x7A, 0xC0, 0xB4, 0x0E, 0x5C, 0xE6, 0x92, 0x28,
     0x24, 0x9E, 0xEA, 0x50, 0x02, 0xB8, 0xCC, 0x76, 0x68, 0xD2, 0xA6, 0x1C, 0x4E, 0xF4, 0x80, 0x3A,
     0xBC, 0x06, 0x72, 0xC8, 0x9A, 0x20, 0x54, 0xEE, 0xF0, 0x4A, 0x3E, 0x84, 0xD6, 0x6C, 0x18, 0xA2,
     0xE6, 0x5C, 0x28, 0x92, 0xC0, 0x7A, 0x0E, 0xB4, 0xAA, 0x10, 0x64, 0xDE, 0x8C, 0x36, 0x42, 0xF8,
     0x7E, 0xC4, 0xB0, 0x0A, 0x58, 0xE2, 0x96, 0x2C, 0x32, 0x88, 0xFC, 0x46, 0x14, 0xAE, 0xDA, 0x60,
     0x6C, 0xD6, 0xA2, 0x18, 0x4A, 0xF0, 0x84, 0x3E, 0x20, 0x9A, 0xEE, 0x54, 0x06, 0xBC, 0xC8, 0x72,
     0xF4, 0x4E, 0x3A, 0x80, 0xD2, 0x68, 0x1C, 0xA6, 0xB8, 0x02, 0x76, 0xCC, 0x9E, 0x24, 0x50, 0xEA,
     0x48, 0xF2, 0x86, 0x3C, 0x6E, 0xD4, 0xA0, 0x1A, 0x04, 0xBE, 0xCA, 0x70, 0x22, 0x98, 0xEC, 0x56,
     0xD0, 0x6A, 0x1E, 0xA4, 0xF6, 0x4C, 0x38, 0x82, 0x9C, 0x26, 0x52, 0xE8, 0xBA, 0x00, 0x74, 0xCE,
     0xC2, 0x78, 0x0C, 0xB6, 0xE4, 0x5E, 0x2A, 0x90, 0x8E, 0x34, 0x40, 0xFA, 0xA8, 0x12, 0x66, 0xDC,
     0x5A, 0xE0, 0x94, 0x2E, 0x7C, 0xC6, 0xB2, 0x08, 0x16, 0xAC, 0xD8, 0x62, 0x30, 0x8A, 0xFE, 0x44};
#endif

HwSerial *CRSF::_dev = NULL;

///Out FIFO to buffer messages///
FIFO SerialOutFIFO;

//U1RXD_IN_IDX
//U1TXD_OUT_IDX

VOLATILE bool CRSF::ignoreSerialData = false;
VOLATILE bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

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

VOLATILE uint8_t CRSF::SerialInPacketStart = 0;                   // length of the CRSF packet as measured
VOLATILE uint8_t CRSF::SerialInPacketLen = 0;                     // length of the CRSF packet as measured
VOLATILE uint8_t CRSF::SerialInPacketPtr = 0;                     // index where we are reading/writing
VOLATILE uint8_t CRSF::SerialInBuffer[CRSF_MAX_PACKET_LEN] = {0}; // max 64 bytes for CRSF packet
VOLATILE uint16_t CRSF::ChannelDataIn[16] = {0};
VOLATILE uint16_t CRSF::ChannelDataInPrev[16] = {0};

// current and sent switch values, used for prioritising sequential switch transmission
uint8_t CRSF::currentSwitches[N_SWITCHES] = {0};
uint8_t CRSF::sentSwitches[N_SWITCHES] = {0};

uint8_t CRSF::nextSwitchIndex = 0; // for round-robin sequential switches

volatile uint8_t CRSF::ParameterUpdateData[2] = {0};

volatile crsf_channels_s CRSF::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;
VOLATILE crsf_sensor_battery_s CRSF::TLMbattSensor;

#if (TX_MODULE)
#if (FEATURE_OPENTX_SYNC)
VOLATILE uint32_t CRSF::RCdataLastRecv = 0;
VOLATILE int32_t CRSF::OpenTXsyncOffset = 0;
VOLATILE uint32_t CRSF::OpenTXsynNextSend = 0;
//VOLATILE uint32_t CRSF::RequestedRCpacketInterval = 10000; // default to 100hz as per 'normal'
VOLATILE uint32_t CRSF::RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
//VOLATILE int32_t CRSF::RequestedRCpacketAdvance = 800;  // timing advance, us
VOLATILE int32_t CRSF::RequestedRCpacketAdvance = 500; // timing advance, us
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

#if (TX_MODULE)
#if (FEATURE_OPENTX_SYNC)
    OpenTXsynNextSend = 0;
    OpenTXsyncOffset = 0;
#endif
#endif

    _dev->flush_read();
}

#if (TX_MODULE)
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

void ICACHE_RAM_ATTR CrsfFramePushToFifo(uint8_t *buff, uint8_t size)
{
    buff[size - 1] = CalcCRC(&buff[2], (buff[1] - 1));
    SerialOutFIFO.push(size); // length
    SerialOutFIFO.pushBytes(buff, size);
}

void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    if (!CRSF::CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // this is nok with volatile
    volatile_memcpy(&outBuffer[3], &LinkStatistics, LinkStatisticsFrameLength);

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
}

void ICACHE_RAM_ATTR sendSetVTXchannel(uint8_t band, uint8_t channel)
{
    if (!CRSF::CRSFstate)
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

void ICACHE_RAM_ATTR CRSF::sendLUAresponse(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4)
{
    if (!CRSF::CRSFstate)
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

void ICACHE_RAM_ATTR CRSF::sendLinkBattSensorToTX()
{
    if (!CRSF::CRSFstate)
        return;

    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(BattSensorFrameLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = CRSF_FRAME_SIZE(BattSensorFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

    outBuffer[3] = (CRSF::TLMbattSensor.voltage >> 8) & 0xff;
    outBuffer[4] = CRSF::TLMbattSensor.voltage & 0xff;
    outBuffer[5] = (CRSF::TLMbattSensor.current >> 8) & 0xff;
    outBuffer[6] = CRSF::TLMbattSensor.current & 0xff;
    outBuffer[7] = (CRSF::TLMbattSensor.capacity >> 16) & 0xff;
    outBuffer[9] = (CRSF::TLMbattSensor.capacity >> 8) & 0xff;
    outBuffer[10] = CRSF::TLMbattSensor.capacity & 0xff;
    outBuffer[11] = CRSF::TLMbattSensor.remaining;

    CrsfFramePushToFifo(outBuffer, sizeof(outBuffer));
    //DEBUG_PRINTLN(CRSFoutBuffer[0]);
}

#if (FEATURE_OPENTX_SYNC)
void ICACHE_RAM_ATTR CRSF::UpdateOpenTxSyncOffset()
{
    CRSF::OpenTXsyncOffset = micros() - CRSF::RCdataLastRecv;
    //DEBUG_PRINTLN(CRSF::OpenTXsyncOffset);
}

void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX() // in values in us.
{
    if (RCdataLastRecv && CRSFstate)
    {
        uint32_t current = millis();
        //DEBUG_PRINT(CRSF::OpenTXsyncOffset);

        if (current >= OpenTXsynNextSend)
        {
            uint32_t packetRate = CRSF::RequestedRCpacketInterval;
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
#endif

bool ICACHE_RAM_ATTR CRSF::TX_ProcessPacket()
{
    bool ret = false;
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

    switch (SerialInBuffer[2])
    {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
    {
        //DEBUG_PRINTLN("Got Other Packet");
        DEBUG_PRINTLN("L");
        if (SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
            SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER)
        {
            ParameterUpdateData[0] = SerialInBuffer[5];
            ParameterUpdateData[1] = SerialInBuffer[6];
            RecvParameterUpdate();
            ret = true;
        }
    }
    case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
    {
        DEBUG_PRINT("X");
#if (FEATURE_OPENTX_SYNC)
        RCdataLastRecv = micros();
#endif
        GetChannelDataIn();
        (RCdataCallback1)(); // run new RC data callback
        (RCdataCallback2)(); // run new RC data callback
        ret = true;
        break;
    }
    default:
        break;
    };
    return ret;
}

void ICACHE_RAM_ATTR CRSF::TX_handleUartIn(void) // Merge with RX version...
{
    uint8_t inChar, split_cnt = 0;

#if (FEATURE_OPENTX_SYNC)
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
                SerialInPacketStart = SerialInPacketPtr - 1;
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
                SerialInPacketStart = SerialInPacketPtr;
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
                if ((SerialInPacketPtr - SerialInPacketStart) == (SerialInPacketLen))
                {
                    uint8_t CalculatedCRC =
                        CalcCRC((uint8_t *)&SerialInBuffer[SerialInPacketStart],
                                (SerialInPacketLen - 1));

                    if (CalculatedCRC == inChar)
                    {
                        if (TX_ProcessPacket())
                        {
                            /* Can write right after successful package reception */
                            _dev->write(SerialOutFIFO);
                        }
                        GoodPktsCount++;
                    }
                    else
                    {
#if 0
                        DEBUG_PRINTLN("UART CRC failure");
                        for (int i = 0; i < (SerialInPacketLen + 2); i++)
                        {
                            DEBUG_PRINT(SerialInBuffer[i], HEX);
                            DEBUG_PRINT(" ");
                        }
                        DEBUG_PRINTLN();
#elif defined(DEBUG_SERIAL)
                        DEBUG_SERIAL.print('C');
                        DEBUG_SERIAL.write((uint8_t *)&SerialInBuffer[SerialInPacketStart - 2], SerialInPacketLen + 2);
#endif
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
            DEBUG_PRINTLN("  Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                SerialOutFIFO.flush();
                DEBUG_PRINTLN("UART WDT: Disconnected");
                disconnected();
                CRSFstate = false;
                OpenTXsynNextSend = 0;
                OpenTXsyncOffset = 0;
            }

            _dev->end();
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

#elif (RX_MODULE) /* END TX_MODULE */

void ICACHE_RAM_ATTR CRSF::CrsfFrameSendToFC(uint8_t *buff, uint8_t size)
{
    buff[size - 1] = CalcCRC(&buff[2], (buff[1] - 1));
    _dev->write(buff, size);
}

void CRSF::sendLinkStatisticsToFC()
{
    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(LinkStatisticsFrameLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    volatile_memcpy(&outBuffer[3], &LinkStatistics, LinkStatisticsFrameLength);

    CrsfFrameSendToFC(outBuffer, sizeof(outBuffer));
}

void ICACHE_RAM_ATTR CRSF::sendRCFrameToFC()
{
    uint8_t outBuffer[CRSF_EXT_FRAME_SIZE(RCframeLength)] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = CRSF_FRAME_SIZE(RCframeLength);
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    volatile_memcpy(&outBuffer[3], &PackedRCdataOut, RCframeLength);

    CrsfFrameSendToFC(outBuffer, sizeof(outBuffer));
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
#endif // END RX_MODULE

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
    VOLATILE uint8_t *ptr = SerialInBuffer;
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
#if (RX_MODULE)
                    RX_processPacket();
#elif (TX_MODULE)
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
