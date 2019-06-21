#include "CRSF.h"
#include "PacketLib.h"
#include <Arduino.h>
#include "HardwareSerial.h"

#ifdef PLATFORM_ESP32
HardwareSerial SerialPort(1);
HardwareSerial CRSF::Port = SerialPort;
#endif

uint8_t CRSF::CSFR_TXpin_Module = 2;
uint8_t CRSF::CSFR_RXpin_Module = 4; // Same pin for RX/TX

volatile bool CRSF::ignoreSerialData = false;
volatile bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

void inline CRSF::nullCallback(void){};

void (*CRSF::RCdataCallback)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint8_t CRSF::SerialInPacketLen = 0;                        // length of the CRSF packet as measured
volatile uint8_t CRSF::SerialInPacketPtr = 0;                        // index where we are reading/writing
volatile uint8_t CRSF::SerialInBuffer[100] = {0};                    // max 64 bytes for CRSF packet
volatile uint8_t CRSF::CRSFoutBuffer[CRSF_MAX_PACKET_LEN + 1] = {0}; // max 64 bytes for CRSF packet
volatile uint16_t CRSF::ChannelDataIn[16] = {0};
volatile uint16_t CRSF::ChannelDataInPrev[16] = {0};

volatile crsf_channels_s CRSF::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;

//CRSF::CRSF(HardwareSerial &serial) : CRSF_SERIAL(serial){};

void CRSF::Begin()
{

#ifdef PLATFORM_ESP32
    //xTaskHandle UartTaskHandle = NULL;
    xTaskCreate(ESP32uartTask, "ESP32uartTask", 20000, NULL, 10, NULL);
#endif
    //The master module requires that the serial communication is bidirectional
    //The Reciever uses seperate rx and tx pins
}

#ifdef PLATFORM_ESP32
void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = CalcCRC(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

    memcpy((uint8_t *)CRSFoutBuffer + 1, outBuffer, LinkStatisticsFrameLength + 4);
    CRSFoutBuffer[0] = LinkStatisticsFrameLength + 4;
    Serial.println(CRSFoutBuffer[0]);
}
#else
void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToFC()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = CalcCRC(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

    this->_dev->write(outBuffer, LinkStatisticsFrameLength + 4);
}
#endif

void ICACHE_RAM_ATTR CRSF::sendRCFrameToFC()
{
    uint8_t outBuffer[RCframeLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = RCframeLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memcpy(outBuffer + 3, (byte *)&PackedRCdataOut, RCframeLength);

    uint8_t crc = CalcCRC(&outBuffer[2], RCframeLength + 1);

    outBuffer[RCframeLength + 3] = crc;

    this->_dev->write(outBuffer, RCframeLength + 4);
}

#ifdef PLATFORM_ESP8266
void ICACHE_RAM_ATTR CRSF::ESP8266ReadUart()
{
}
#endif

#ifdef PLATFORM_ESP32
void ICACHE_RAM_ATTR CRSF::ESP32uartTask(void *pvParameters) //RTOS task to read and write CRSF packets to the serial port
{
    const TickType_t xDelay1 = 1 / portTICK_PERIOD_MS;
    const TickType_t xDelay2 = 2 / portTICK_PERIOD_MS;
    const TickType_t xDelay3 = 3 / portTICK_PERIOD_MS;
    const TickType_t xDelay4 = 4 / portTICK_PERIOD_MS;
    const TickType_t xDelay5 = 5 / portTICK_PERIOD_MS;
    CRSF::Port.begin(CRSF_OPENTX_SLOW_BAUDRATE, SERIAL_8N1, CSFR_RXpin_Module, CSFR_TXpin_Module, true);
    Serial.println("ESP32 UART LISTEN TASK STARTED");

    uint32_t LastDataTime = millis();
    uint32_t YieldInterval = 100;

    for (;;)
    {
        if (millis() > (LastDataTime + YieldInterval))
        { //prevents WDT reset by yielding when there is no CRSF data to proces
            vTaskDelay(YieldInterval / portTICK_PERIOD_MS);
        }

        if (CRSF::Port.available())
        {
            LastDataTime = millis();
            char inChar = CRSF::Port.read();

            if ((inChar == CRSF_ADDRESS_CRSF_TRANSMITTER || CRSF_SYNC_BYTE) && CRSFframeActive == false) // we got sync, reset write pointer
            {
                SerialInPacketPtr = 0;
                CRSFframeActive = true;
            }

            if (SerialInPacketPtr == 1 && CRSFframeActive == true) // we read the packet length and save it
            {
                SerialInPacketLen = inChar;
            }

            if (SerialInPacketPtr == 64) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
            {
                SerialInPacketPtr = 0;
            }

            SerialInBuffer[SerialInPacketPtr] = inChar;
            SerialInPacketPtr++;

            if (SerialInPacketPtr == SerialInPacketLen + 2) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
            {
                char CalculatedCRC = CalcCRC((uint8_t *)SerialInBuffer + 2, SerialInPacketPtr - 3);
                if (CalculatedCRC == inChar)
                {
                    ProcessPacket();
                    SerialInPacketPtr = 0;
                    CRSFframeActive = false;
                    vTaskDelay(xDelay2);
                    if (CRSFoutBuffer[0] > 0)
                    {
                        CRSF::Port.write((uint8_t *)CRSFoutBuffer + 1, CRSFoutBuffer[0]);
                        memset((uint8_t *)CRSFoutBuffer, 0, CRSFoutBuffer[0] + 1);
                    }
                    FlushSerial();
                }
                else
                {

                    Serial.println("CRC failure");
                    Serial.println(SerialInPacketPtr, HEX);
                    Serial.print("Expected: ");
                    Serial.println(CalculatedCRC, HEX);
                    Serial.print("Got: ");
                    Serial.println(inChar, HEX);
                    for (int i = 0; i < SerialInPacketLen + 2; i++)
                    {
                        Serial.print(SerialInBuffer[i], HEX);
                        Serial.print(" ");
                    }
                    Serial.println();
                    Serial.println();
                    CRSFframeActive = false;
                    SerialInPacketPtr = 0;
                    FlushSerial();
                    //vTaskDelay(xDelay);
                }
            }
        }
    }
}
#endif

void ICACHE_RAM_ATTR CRSF::ProcessPacket()
{

    if (CRSF::SerialInBuffer[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        GetChannelDataIn();
        (RCdataCallback)(); // run new RC data callback
    }
}

void ICACHE_RAM_ATTR CRSF::GetChannelDataIn() // data is packed as 11 bits per channel
{
#define SERIAL_PACKET_OFFSET 3

    // for (int i = 0; i < 16; i++)
    //{
    memcpy((uint16_t *)ChannelDataInPrev, (uint16_t *)ChannelDataIn, 16); //before we write the new RC channel data copy the old data
    //this is used in other code to detect when the channel data has changed
    // }

    const crsf_channels_t *const rcChannels = (crsf_channels_t *)&CRSF::SerialInBuffer[SERIAL_PACKET_OFFSET];
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
}

void ICACHE_RAM_ATTR CRSF::FlushSerial()
{
    while (CRSF::Port.available())
    {
        CRSF::Port.read();
    }
}