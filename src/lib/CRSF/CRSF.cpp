#include "CRSF.h"
#include "device.h"
#include "FIFO.h"
#include "logging.h"
#include "helpers.h"

#if defined(CRSF_TX_MODULE)
#if defined(PLATFORM_ESP32)
#include <soc/uart_reg.h>
// UART0 is used since for DupleTX we can connect directly through IO_MUX and not the Matrix
// for better performance, and on other targets (mostly using pin 13), it always uses Matrix
HardwareSerial CRSF::Port(0);
portMUX_TYPE FIFOmux = portMUX_INITIALIZER_UNLOCKED;

RTC_DATA_ATTR int rtcModelId = 0;
#elif defined(PLATFORM_ESP8266)
HardwareSerial CRSF::Port(0);
#elif CRSF_TX_MODULE_STM32
HardwareSerial CRSF::Port(GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
#if defined(STM32F3) || defined(STM32F3xx)
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#elif defined(STM32F1) || defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#endif
#elif defined(TARGET_NATIVE)
HardwareSerial CRSF::Port = Serial;
#endif
#endif

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

/// Out FIFO to buffer messages///
static FIFO SerialOutFIFO;

inBuffer_U CRSF::inBuffer;

volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;

#if CRSF_TX_MODULE
#define HANDSET_TELEMETRY_FIFO_SIZE 128 // this is the smallest telemetry FIFO size in ETX with CRSF defined

Stream *CRSF::PortSecondary;
static FIFO MspWriteFIFO;

void (*CRSF::disconnected)() = nullptr; // called when CRSF stream is lost
void (*CRSF::connected)() = nullptr;    // called when CRSF stream is regained

void (*CRSF::RecvParameterUpdate)(uint8_t type, uint8_t index, uint8_t arg) = nullptr; // called when recv parameter update req, ie from LUA
void (*CRSF::RecvModelUpdate)() = nullptr; // called when model id cahnges, ie command from Radio
void (*CRSF::RCdataCallback)() = nullptr; // called when there is new RC data

/// UART Handling ///
uint8_t CRSF::SerialInPacketLen = 0; // length of the CRSF packet as measured
uint8_t CRSF::SerialInPacketPtr = 0; // index where we are reading/writing
bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

uint32_t CRSF::GoodPktsCountResult = 0;
uint32_t CRSF::BadPktsCountResult = 0;

uint8_t CRSF::modelId = 0;
bool CRSF::ForwardDevicePings = false;
bool CRSF::elrsLUAmode = false;

/// OpenTX mixer sync ///
uint32_t CRSF::OpenTXsyncLastSent = 0;
uint32_t CRSF::RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
volatile uint32_t CRSF::RCdataLastRecv = 0;
volatile int32_t CRSF::OpenTXsyncOffset = 0;
volatile int32_t CRSF::OpenTXsyncWindow = 0;
volatile int32_t CRSF::OpenTXsyncWindowSize = 0;
volatile uint32_t CRSF::dataLastRecv = 0;
bool CRSF::OpentxSyncActive = true;
uint32_t CRSF::OpenTXsyncOffsetSafeMargin = 1000; // 100us

/// UART Handling ///
uint32_t CRSF::GoodPktsCount = 0;
uint32_t CRSF::BadPktsCount = 0;
uint32_t CRSF::UARTwdtLastChecked;

uint8_t CRSF::CRSFoutBuffer[CRSF_MAX_PACKET_LEN] = {0};
uint8_t CRSF::maxPacketBytes = CRSF_MAX_PACKET_LEN;
uint8_t CRSF::maxPeriodBytes = CRSF_MAX_PACKET_LEN;
uint32_t CRSF::TxToHandsetBauds[] = {400000, 115200, 5250000, 3750000, 1870000, 921600, 2250000};
uint8_t CRSF::UARTcurrentBaudIdx = 0;
uint32_t CRSF::UARTrequestedBaud = 400000;
#if defined(PLATFORM_ESP32)
bool CRSF::UARTinverted = false;
#endif

bool CRSF::CRSFstate = false;

// for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000

uint8_t CRSF::MspData[ELRS_MSP_BUFFER] = {0};
uint8_t CRSF::MspDataLength = 0;
#endif // CRSF_TX_MODULE

void CRSF::Begin()
{
    DBGLN("About to start CRSF task...");

#if CRSF_TX_MODULE
    UARTwdtLastChecked = millis() + UARTwdtInterval; // allows a delay before the first time the UARTwdt() function is called

#if defined(PLATFORM_ESP32)
    portDISABLE_INTERRUPTS();
    UARTinverted = firmwareOptions.uart_inverted;
    CRSF::Port.begin(TxToHandsetBauds[UARTcurrentBaudIdx], SERIAL_8N1,
                     GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX,
                     false, 500);
    CRSF::duplex_set_RX();
    portENABLE_INTERRUPTS();
    flush_port_input();
    if (esp_reset_reason() != ESP_RST_POWERON)
    {
        modelId = rtcModelId;
        if (RecvModelUpdate) RecvModelUpdate();
    }
#elif defined(PLATFORM_ESP8266)
    // Uses default UART pins
    CRSF::Port.begin(TxToHandsetBauds[UARTcurrentBaudIdx]);
    // Invert RX/TX (not done, connection is full duplex uninverted)
    //USC0(UART0) |= BIT(UCRXI) | BIT(UCTXI);
    // No log message because this is our only UART

#elif defined(PLATFORM_STM32)
    DBGLN("Start STM32 R9M TX CRSF UART");

    CRSF::Port.setTx(GPIO_PIN_RCSIGNAL_TX);
    CRSF::Port.setRx(GPIO_PIN_RCSIGNAL_RX);

    #if defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    pinMode(GPIO_PIN_BUFFER_OE, OUTPUT);
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED); // RX mode default
    #elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    CRSF::Port.setHalfDuplex();
    #endif

    CRSF::Port.begin(TxToHandsetBauds[UARTcurrentBaudIdx]);

#if defined(TARGET_TX_GHOST)
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR3 |= USART_CR3_HDSEL;
    USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
    USART1->CR1 |= USART_CR1_UE;
#endif
#if defined(TARGET_TX_FM30_MINI)
    LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
    USART2->CR1 &= ~USART_CR1_UE;
    USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
    USART2->CR1 |= USART_CR1_UE;
#endif
    DBGLN("STM32 CRSF UART LISTEN TASK STARTED");
    CRSF::Port.flush();
    flush_port_input();
#endif

#endif // CRSF_TX_MODULE

    //The master module requires that the serial communication is bidirectional
    //The Reciever uses seperate rx and tx pins
}

void CRSF::End()
{
#if CRSF_TX_MODULE
    uint32_t startTime = millis();
    while (SerialOutFIFO.peek() > 0)
    {
        handleUARTin();
        if (millis() - startTime > 1000)
        {
            break;
        }
    }
    //CRSF::Port.end(); // don't call seria.end(), it causes some sort of issue with the 900mhz hardware using gpio2 for serial
    DBGLN("CRSF UART END");
#endif // CRSF_TX_MODULE
}

#if CRSF_TX_MODULE
void CRSF::flush_port_input(void)
{
    // Make sure there is no garbage on the UART at the start
    while (CRSF::Port.available())
    {
        CRSF::Port.read();
    }
}

void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    if (!CRSF::CRSFstate)
    {
        return;
    }

    constexpr uint8_t outBuffer[4] = {
        LinkStatisticsFrameLength + 4,
        CRSF_ADDRESS_RADIO_TRANSMITTER,
        LinkStatisticsFrameLength + 2,
        CRSF_FRAMETYPE_LINK_STATISTICS
    };

    uint8_t crc = crsf_crc.calc(outBuffer[3]);
    crc = crsf_crc.calc((byte *)&LinkStatistics, LinkStatisticsFrameLength, crc);

#ifdef PLATFORM_ESP32
    portENTER_CRITICAL(&FIFOmux);
#endif
    if (SerialOutFIFO.ensure(outBuffer[0] + 1))
    {
        SerialOutFIFO.pushBytes(outBuffer, sizeof(outBuffer));
        SerialOutFIFO.pushBytes((byte *)&LinkStatistics, LinkStatisticsFrameLength);
        SerialOutFIFO.push(crc);
    }
#ifdef PLATFORM_ESP32
    portEXIT_CRITICAL(&FIFOmux);
#endif
}

/**
 * Build a an extended type packet and queue it in the SerialOutFIFO
 * This is just a regular packet with 2 extra bytes with the sub src and target
 **/
void CRSF::packetQueueExtended(uint8_t type, void *data, uint8_t len)
{
    if (!CRSF::CRSFstate)
        return;

    uint8_t buf[6] = {
        (uint8_t)(len + 6),
        CRSF_ADDRESS_RADIO_TRANSMITTER,
        (uint8_t)(len + 4),
        type,
        CRSF_ADDRESS_RADIO_TRANSMITTER,
        CRSF_ADDRESS_CRSF_TRANSMITTER
    };

    // CRC - Starts at type, ends before CRC
    uint8_t crc = crsf_crc.calc(&buf[3], sizeof(buf)-3);
    crc = crsf_crc.calc((byte *)data, len, crc);

#ifdef PLATFORM_ESP32
    portENTER_CRITICAL(&FIFOmux);
#endif
    if (SerialOutFIFO.ensure(buf[0] + 1))
    {
        SerialOutFIFO.pushBytes(buf, sizeof(buf));
        SerialOutFIFO.pushBytes((byte *)data, len);
        SerialOutFIFO.push(crc);
    }
#ifdef PLATFORM_ESP32
    portEXIT_CRITICAL(&FIFOmux);
#endif
}

void ICACHE_RAM_ATTR CRSF::sendTelemetryToTX(uint8_t *data)
{
    if (CRSF::CRSFstate)
    {
        uint8_t size = CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX]);
        if (size > CRSF_MAX_PACKET_LEN)
        {
            ERRLN("too large");
            return;
        }

        data[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
#ifdef PLATFORM_ESP32
        portENTER_CRITICAL(&FIFOmux);
#endif
        if (SerialOutFIFO.ensure(size + 1))
        {
            SerialOutFIFO.push(size); // length
            SerialOutFIFO.pushBytes(data, size);
        }
#ifdef PLATFORM_ESP32
        portEXIT_CRITICAL(&FIFOmux);
#endif
    }
}

void ICACHE_RAM_ATTR CRSF::setSyncParams(uint32_t PacketInterval)
{
    CRSF::RequestedRCpacketInterval = PacketInterval;
    CRSF::OpenTXsyncOffset = 0;
    CRSF::OpenTXsyncWindow = 0;
    CRSF::OpenTXsyncWindowSize = std::max((int32_t)1, (int32_t)(20000/CRSF::RequestedRCpacketInterval));
    CRSF::OpenTXsyncLastSent -= OpenTXsyncPacketInterval;
    adjustMaxPacketSize();
}

uint32_t ICACHE_RAM_ATTR CRSF::GetRCdataLastRecv()
{
    return CRSF::RCdataLastRecv;
}

void ICACHE_RAM_ATTR CRSF::JustSentRFpacket()
{
    // read them in this order to prevent a potential race condition
    uint32_t last = CRSF::dataLastRecv;
    uint32_t m = micros();
    int32_t delta = (int32_t)(m - last);

    if (delta >= (int32_t)CRSF::RequestedRCpacketInterval)
    {
        // missing/late packet, force resync
        CRSF::OpenTXsyncOffset = -(delta % CRSF::RequestedRCpacketInterval) * 10;
        CRSF::OpenTXsyncWindow = 0;
        CRSF::OpenTXsyncLastSent -= OpenTXsyncPacketInterval;
#ifdef DEBUG_OPENTX_SYNC
        DBGLN("Missed packet, forced resync (%d)!", delta);
#endif
    }
    else
    {
        // The number of packets in the sync window is how many will fit in 20ms.
        // This gives quite quite coarse changes for 50Hz, but more fine grained changes at 1000Hz.
        CRSF::OpenTXsyncWindow = std::min(CRSF::OpenTXsyncWindow + 1, (int32_t)CRSF::OpenTXsyncWindowSize);
        CRSF::OpenTXsyncOffset = ((CRSF::OpenTXsyncOffset * (CRSF::OpenTXsyncWindow-1)) + delta * 10) / CRSF::OpenTXsyncWindow;
    }
}

void CRSF::disableOpentxSync()
{
    OpentxSyncActive = false;
}

void CRSF::enableOpentxSync()
{
    OpentxSyncActive = true;
}

void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX() // in values in us.
{
    uint32_t now = millis();
    if (CRSF::CRSFstate && (now - OpenTXsyncLastSent) >= OpenTXsyncPacketInterval)
    {
        uint32_t packetRate = CRSF::RequestedRCpacketInterval * 10; //convert from us to right format
        int32_t offset = CRSF::OpenTXsyncOffset - CRSF::OpenTXsyncOffsetSafeMargin; // offset so that opentx always has some headroom
#ifdef DEBUG_OPENTX_SYNC
        DBGLN("Offset %d", offset); // in 10ths of us (OpenTX sync unit)
#endif

        struct otxSyncData {
            uint8_t extendedType; // CRSF_FRAMETYPE_OPENTX_SYNC
            uint32_t rate; // Big-Endian
            uint32_t offset; // Big-Endian
        } PACKED;

        uint8_t buffer[sizeof(otxSyncData)];
        struct otxSyncData * const sync = (struct otxSyncData * const)buffer;

        sync->extendedType = CRSF_FRAMETYPE_OPENTX_SYNC;
        sync->rate = htobe32(packetRate);
        sync->offset = htobe32(offset);

        packetQueueExtended(CRSF_FRAMETYPE_RADIO_ID, buffer, sizeof(buffer));

        OpenTXsyncLastSent = now;
    }
}

void ICACHE_RAM_ATTR CRSF::RcPacketToChannelsData() // data is packed as 11 bits per channel
{
    // for monitoring arming state
    uint32_t prev_AUX1 = ChannelData[4];

    uint8_t const * const payload = (uint8_t const * const)&CRSF::inBuffer.asRCPacket_t.channels;
    constexpr unsigned srcBits = 11;
    constexpr unsigned dstBits = 11;
    constexpr unsigned inputChannelMask = (1 << srcBits) - 1;
    constexpr unsigned precisionShift = dstBits - srcBits;

    // code from BetaFlight rx/crsf.cpp / bitpacker_unpack
    uint8_t bitsMerged = 0;
    uint32_t readValue = 0;
    unsigned readByteIndex = 0;
    for (unsigned n = 0; n < CRSF_NUM_CHANNELS; n++)
    {
        while (bitsMerged < srcBits)
        {
            uint8_t readByte = payload[readByteIndex++];
            readValue |= ((uint32_t) readByte) << bitsMerged;
            bitsMerged += 8;
        }
        //printf("rv=%x(%x) bm=%u\n", readValue, (readValue & inputChannelMask), bitsMerged);
        ChannelData[n] = (readValue & inputChannelMask) << precisionShift;
        readValue >>= srcBits;
        bitsMerged -= srcBits;
    }

    if (prev_AUX1 != ChannelData[4])
    {
    #if defined(PLATFORM_ESP32)
        devicesTriggerEvent();
    #endif
    }
}

bool ICACHE_RAM_ATTR CRSF::ProcessPacket()
{
    bool packetReceived = false;

    CRSF::dataLastRecv = micros();

    if (CRSFstate == false)
    {
        CRSFstate = true;
        DBGLN("CRSF UART Connected");
        if (connected) connected();
    }

    const uint8_t packetType = CRSF::inBuffer.asRCPacket_t.header.type;
    uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;

    if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        CRSF::RCdataLastRecv = micros();
        RcPacketToChannelsData();
        packetReceived = true;
    }
    // check for all extended frames that are a broadcast or a message to the FC
    else if (packetType >= CRSF_FRAMETYPE_DEVICE_PING &&
            (SerialInBuffer[3] == CRSF_ADDRESS_FLIGHT_CONTROLLER || SerialInBuffer[3] == CRSF_ADDRESS_BROADCAST || SerialInBuffer[3] == CRSF_ADDRESS_CRSF_RECEIVER))
    {
        // Some types trigger telemburst to attempt a connection even with telm off
        // but for pings (which are sent when the user loads Lua) do not forward
        // unless connected
        if (ForwardDevicePings || packetType != CRSF_FRAMETYPE_DEVICE_PING)
        {
            const uint8_t length = CRSF::inBuffer.asRCPacket_t.header.frame_size + 2;
            AddMspMessage(length, SerialInBuffer);
        }
        packetReceived = true;
    }

    // always execute this check since broadcast needs to be handled in all cases
    if (packetType >= CRSF_FRAMETYPE_DEVICE_PING &&
        (SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER || SerialInBuffer[3] == CRSF_ADDRESS_BROADCAST) &&
        (SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER || SerialInBuffer[4] == CRSF_ADDRESS_ELRS_LUA))
    {
        elrsLUAmode = SerialInBuffer[4] == CRSF_ADDRESS_ELRS_LUA;

        if (packetType == CRSF_FRAMETYPE_COMMAND && SerialInBuffer[5] == SUBCOMMAND_CRSF && SerialInBuffer[6] == COMMAND_MODEL_SELECT_ID)
        {
            modelId = SerialInBuffer[7];
            #if defined(PLATFORM_ESP32)
            rtcModelId = modelId;
            #endif
            if (RecvModelUpdate) RecvModelUpdate();
        }
        else
        {
            if (RecvParameterUpdate) RecvParameterUpdate(packetType, SerialInBuffer[5], SerialInBuffer[6]);
        }

        packetReceived = true;
    }

    return packetReceived;
}

void CRSF::GetMspMessage(uint8_t **data, uint8_t *len)
{
    *len = MspDataLength;
    *data = (MspDataLength > 0) ? MspData : nullptr;
}

void CRSF::ResetMspQueue()
{
    MspWriteFIFO.flush();
    MspDataLength = 0;
    memset(MspData, 0, ELRS_MSP_BUFFER);
}

void CRSF::UnlockMspMessage()
{
    // current msp message is sent so restore next buffered write
    if (MspWriteFIFO.peek() > 0)
    {
        uint8_t length = MspWriteFIFO.pop();
        MspDataLength = length;
        MspWriteFIFO.popBytes(MspData, length);
    }
    else
    {
        // no msp message is ready to send currently
        MspDataLength = 0;
        memset(MspData, 0, ELRS_MSP_BUFFER);
    }
}

void ICACHE_RAM_ATTR CRSF::AddMspMessage(mspPacket_t* packet)
{
    if (packet->payloadSize > ENCAPSULATED_MSP_MAX_PAYLOAD_SIZE)
    {
        return;
    }

    const uint8_t totalBufferLen = packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_NOT_COUNTED_BYTES;
    uint8_t outBuffer[ENCAPSULATED_MSP_MAX_FRAME_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_NOT_COUNTED_BYTES];

    // CRSF extended frame header
    outBuffer[0] = CRSF_ADDRESS_BROADCAST;                                      // address
    outBuffer[1] = packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC; // length
    outBuffer[2] = CRSF_FRAMETYPE_MSP_WRITE;                                    // packet type
    outBuffer[3] = CRSF_ADDRESS_FLIGHT_CONTROLLER;                              // destination
    outBuffer[4] = CRSF_ADDRESS_RADIO_TRANSMITTER;                              // origin

    // Encapsulated MSP payload
    outBuffer[5] = 0x30;                // header
    outBuffer[6] = packet->payloadSize; // mspPayloadSize
    outBuffer[7] = packet->function;    // packet->cmd
    for (uint8_t i = 0; i < packet->payloadSize; ++i)
    {
        // copy packet payload into outBuffer
        outBuffer[8 + i] = packet->payload[i];
    }
    // Encapsulated MSP crc
    outBuffer[totalBufferLen - 2] = CalcCRCMsp(&outBuffer[6], packet->payloadSize + 2);

    // CRSF frame crc
    outBuffer[totalBufferLen - 1] = crsf_crc.calc(&outBuffer[2], packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC - 1);
    AddMspMessage(totalBufferLen, outBuffer);
}

void ICACHE_RAM_ATTR CRSF::AddMspMessage(const uint8_t length, uint8_t* data)
{
    if (length > ELRS_MSP_BUFFER)
    {
        return;
    }

    // store next msp message
    if (MspDataLength == 0)
    {
        for (uint8_t i = 0; i < length; i++)
        {
            MspData[i] = data[i];
        }
        MspDataLength = length;
    }
    // store all write requests since an update does send multiple writes
    else
    {
        if (MspWriteFIFO.ensure(length + 1))
        {
            MspWriteFIFO.push(length);
            MspWriteFIFO.pushBytes((const uint8_t *)data, length);
        }
    }
}

void ICACHE_RAM_ATTR CRSF::handleUARTin()
{
    uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;

    if (UARTwdt())
    {
        return;
    }

    while (CRSF::Port.available())
    {
        if (CRSFframeActive == false)
        {
            unsigned char const inChar = CRSF::Port.read();
            // stage 1 wait for sync byte //
            if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER ||
                inChar == CRSF_SYNC_BYTE)
            {
                // we got sync, reset write pointer
                SerialInPacketPtr = 0;
                SerialInPacketLen = 0;
                CRSFframeActive = true;
                SerialInBuffer[SerialInPacketPtr] = inChar;
                SerialInPacketPtr++;
            }
        }
        else // frame is active so we do the processing
        {
            // first if things have gone wrong //
            if (SerialInPacketPtr > CRSF_MAX_PACKET_LEN - 1)
            {
                // we reached the maximum allowable packet length, so start again because shit fucked up hey.
                SerialInPacketPtr = 0;
                SerialInPacketLen = 0;
                CRSFframeActive = false;
                return;
            }

            // special case where we save the expected pkt len to buffer //
            if (SerialInPacketPtr == 1)
            {
                unsigned char const inChar = CRSF::Port.read();
                if (inChar <= CRSF_MAX_PACKET_LEN)
                {
                    SerialInPacketLen = inChar;
                    SerialInBuffer[SerialInPacketPtr] = inChar;
                    SerialInPacketPtr++;
                }
                else
                {
                    SerialInPacketPtr = 0;
                    SerialInPacketLen = 0;
                    CRSFframeActive = false;
                    return;
                }
            }

            int toRead = (SerialInPacketLen + 2) - SerialInPacketPtr;
            #if defined(PLATFORM_ESP32)
            int count = CRSF::Port.read(&SerialInBuffer[SerialInPacketPtr], toRead);
            #else
            int count = 0;
            int avail = CRSF::Port.available();
            while (count < toRead && count < avail)
            {
                SerialInBuffer[SerialInPacketPtr + count] = CRSF::Port.read();
                count++;
            }
            #endif
            SerialInPacketPtr += count;

            if (SerialInPacketPtr >= (SerialInPacketLen + 2)) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
            {
                char CalculatedCRC = crsf_crc.calc(SerialInBuffer + 2, SerialInPacketPtr - 3);

                if (CalculatedCRC == SerialInBuffer[SerialInPacketPtr-1])
                {
                    GoodPktsCount++;
                    if (ProcessPacket())
                    {
                        //delayMicroseconds(50);
                        handleUARTout();
                        if (RCdataCallback) RCdataCallback();
                    }
                }
                else
                {
                    DBGLN("UART CRC failure");
                    // cleanup input buffer
                    flush_port_input();
                    BadPktsCount++;
                }
                CRSFframeActive = false;
                SerialInPacketPtr = 0;
                SerialInPacketLen = 0;
            }
        }
    }
}

void ICACHE_RAM_ATTR CRSF::handleUARTout()
{
    // both static to split up larger packages
    static uint8_t packageLengthRemaining = 0;
    static uint8_t sendingOffset = 0;

    if (OpentxSyncActive && packageLengthRemaining == 0 && SerialOutFIFO.size() == 0)
    {
        sendSyncPacketToTX(); // calculate mixer sync packet if needed
    }

    // if partial package remaining, or data in the output FIFO that needs to be written
    if (packageLengthRemaining > 0 || SerialOutFIFO.size() > 0) {
        duplex_set_TX();

        uint8_t periodBytesRemaining = maxPeriodBytes;
        while (periodBytesRemaining)
        {
#ifdef PLATFORM_ESP32
            portENTER_CRITICAL(&FIFOmux); // stops other tasks from writing to the FIFO when we want to read it
#endif
            // no package is in transit so get new data from the fifo
            if (packageLengthRemaining == 0) {
                packageLengthRemaining = SerialOutFIFO.pop();
                SerialOutFIFO.popBytes(CRSFoutBuffer, packageLengthRemaining);
                sendingOffset = 0;
            }
#ifdef PLATFORM_ESP32
            portEXIT_CRITICAL(&FIFOmux); // stops other tasks from writing to the FIFO when we want to read it
#endif

            // if the package is long we need to split it up so it fits in the sending interval
            uint8_t writeLength;
            if (packageLengthRemaining > periodBytesRemaining) {
                if (periodBytesRemaining < maxPeriodBytes) {  // only start to send a split packet as the first packet
                    break;
                }
                writeLength = periodBytesRemaining;
            } else {
                writeLength = packageLengthRemaining;
            }

            // write the packet out, if it's a large package the offset holds the starting position
            CRSF::Port.write(CRSFoutBuffer + sendingOffset, writeLength);
            if (CRSF::PortSecondary)
                CRSF::PortSecondary->write(CRSFoutBuffer + sendingOffset, writeLength);

            sendingOffset += writeLength;
            packageLengthRemaining -= writeLength;
            periodBytesRemaining -= writeLength;

            // No bytes left to send, exit
            if (SerialOutFIFO.size() == 0)
                break;
        }
        CRSF::Port.flush();
        duplex_set_RX();

        // make sure there is no garbage on the UART left over
        flush_port_input();
    }
}

void ICACHE_RAM_ATTR CRSF::duplex_set_RX()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    {
        ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
        if (UARTinverted)
        {
            gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U0RXD_IN_IDX, true);
            gpio_pulldown_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
            gpio_pullup_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
        }
        else
        {
            gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U0RXD_IN_IDX, false);
            gpio_pullup_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
            gpio_pulldown_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
        }
    }
#elif defined(PLATFORM_ESP8266)
    // Enable loopback on UART0 to connect the RX pin to the TX pin (not done, connection is full duplex uninverted)
    //USC0(UART0) |= BIT(UCLBE);
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED);
#elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    CRSF::Port.enableHalfDuplexRx();
#endif
}

void ICACHE_RAM_ATTR CRSF::duplex_set_TX()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    {
        ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_FLOATING));
        ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
        if (UARTinverted)
        {
            ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0));
            ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
            constexpr uint8_t MATRIX_DETACH_IN_LOW = 0x30; // routes 0 to matrix slot
            gpio_matrix_in(MATRIX_DETACH_IN_LOW, U0RXD_IN_IDX, false); // Disconnect RX from all pads
            gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U0TXD_OUT_IDX, true, false);
        }
        else
        {
            ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 1));
            ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
            constexpr uint8_t MATRIX_DETACH_IN_HIGH = 0x38; // routes 1 to matrix slot
            gpio_matrix_in(MATRIX_DETACH_IN_HIGH, U0RXD_IN_IDX, false); // Disconnect RX from all pads
            gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U0TXD_OUT_IDX, false, false);
        }
    }
#elif defined(PLATFORM_ESP8266)
    // Disable loopback to disconnect the RX pin from the TX pin (not done, connection is full duplex uninverted)
    //USC0(UART0) &= ~BIT(UCLBE);
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, HIGH ^ GPIO_PIN_BUFFER_OE_INVERTED);
#elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    // writing to the port switches the mode
#endif
}

void ICACHE_RAM_ATTR CRSF::adjustMaxPacketSize()
{
    // baud / 10bits-per-byte / 2 windows (1RX, 1TX) / rate * 0.80 (leeway)
    maxPeriodBytes = UARTrequestedBaud / 10 / 2 / (1000000/RequestedRCpacketInterval) * 80 / 100;
    maxPeriodBytes = maxPeriodBytes > HANDSET_TELEMETRY_FIFO_SIZE ? HANDSET_TELEMETRY_FIFO_SIZE : maxPeriodBytes;
    // we need a minimum of 10 bytes otherwise our LUA will not make progress and at 8 we'd get a divide by 0!
    maxPeriodBytes = maxPeriodBytes < 10 ? 10 : maxPeriodBytes;
    maxPacketBytes = maxPeriodBytes > CRSF_MAX_PACKET_LEN ? CRSF_MAX_PACKET_LEN : maxPeriodBytes;
    DBGLN("Adjusted max packet size %u-%u", maxPacketBytes, maxPeriodBytes);
}

#if defined(PLATFORM_ESP32)
uint32_t CRSF::autobaud()
{
    static enum { INIT, MEASURED, INVERTED } state;

    uint32_t *autobaud_reg = (uint32_t *)UART_AUTOBAUD_REG(0);
    uint32_t *rxd_cnt_reg = (uint32_t *)UART_RXD_CNT_REG(0);

    if (state == MEASURED) {
        UARTinverted = !UARTinverted;
        state = INVERTED;
        return UARTrequestedBaud;
    } else if (state == INVERTED) {
        UARTinverted = !UARTinverted;
        state = INIT;
    }

    if ((*autobaud_reg & 1) == 0) {
        *autobaud_reg = (4 << 8) | 1;    // enable, glitch filter 4
        return 400000;
    } else if ((*autobaud_reg & 1) && (*rxd_cnt_reg < 300))
        return 400000;

    state = MEASURED;

    uint32_t low_period  = *(uint32_t *)UART_LOWPULSE_REG(0);
    uint32_t high_period = *(uint32_t *)UART_HIGHPULSE_REG(0);
    *autobaud_reg = (4 << 8) | 0;

    DBGLN("autobaud: low %d, high %d", low_period, high_period);
    // sample code at https://github.com/espressif/esp-idf/issues/3336
    // says baud rate = 80000000/min(UART_LOWPULSE_REG, UART_HIGHPULSE_REG);
    // Based on testing use max and add 2 for lowest deviation
    int32_t calulatedBaud = 80000000 / (max(low_period, high_period) + 2);
    int32_t bestBaud = (int32_t)TxToHandsetBauds[0];
    for(int i=0 ; i<ARRAY_SIZE(TxToHandsetBauds) ; i++)
    {
        if (abs(calulatedBaud - bestBaud) > abs(calulatedBaud - (int32_t)TxToHandsetBauds[i]))
        {
            bestBaud = (int32_t)TxToHandsetBauds[i];
        }
    }
    return bestBaud;
}
#else
uint32_t CRSF::autobaud() {
    UARTcurrentBaudIdx = (UARTcurrentBaudIdx + 1) % ARRAY_SIZE(TxToHandsetBauds);
    return TxToHandsetBauds[UARTcurrentBaudIdx];
}
#endif

bool CRSF::UARTwdt()
{
    bool retval = false;
#if !defined(DEBUG_TX_FREERUN)
    uint32_t now = millis();
    if (now >= (UARTwdtLastChecked + UARTwdtInterval))
    {
        if (BadPktsCount >= GoodPktsCount)
        {
            DBGLN("Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                DBGLN("CRSF UART Disconnected");
                if (disconnected) disconnected();
                CRSFstate = false;
            }

            UARTrequestedBaud = autobaud();

            DBGLN("UART WDT: Switch to: %d baud", UARTrequestedBaud);

            adjustMaxPacketSize();

            SerialOutFIFO.flush();
#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
            CRSF::Port.flush();
            CRSF::Port.updateBaudRate(UARTrequestedBaud);
#elif defined(TARGET_TX_GHOST)
            CRSF::Port.begin(UARTrequestedBaud);
            USART1->CR1 &= ~USART_CR1_UE;
            USART1->CR3 |= USART_CR3_HDSEL;
            USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
            USART1->CR1 |= USART_CR1_UE;
#elif defined(TARGET_TX_FM30_MINI)
            CRSF::Port.begin(UARTrequestedBaud);
            LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
            USART2->CR1 &= ~USART_CR1_UE;
            USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
            USART2->CR1 |= USART_CR1_UE;
#else
            CRSF::Port.begin(UARTrequestedBaud);
#endif
            duplex_set_RX();
            // cleanup input buffer
            flush_port_input();

            retval = true;
        }
#ifdef DEBUG_OPENTX_SYNC
        if (abs((int)((1000000 / (ExpressLRS_currAirRate_Modparams->interval * ExpressLRS_currAirRate_Modparams->numOfSends)) - (int)GoodPktsCount)) > 1)
#endif
            DBGLN("UART STATS Bad:Good = %u:%u", BadPktsCount, GoodPktsCount);

        UARTwdtLastChecked = now;
        if (retval)
        {
            // Speed up the cycling
            UARTwdtLastChecked -= 3 * (UARTwdtInterval >> 2);
        }

        GoodPktsCountResult = GoodPktsCount;
        BadPktsCountResult = BadPktsCount;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
#endif
    return retval;
}
#endif // CRSF_RX_MODULE

/***
 * @brief: Convert `version` (string) to a integer version representation
 * e.g. "2.2.15 ISM24G" => 0x0002020f
 * Assumes all version fields are < 256, the number portion
 * MUST be followed by a space to correctly be parsed
 ***/
uint32_t CRSF::VersionStrToU32(const char *verStr)
{
    uint32_t retVal = 0;
#if !defined(FORCE_NO_DEVICE_VERSION)
    uint8_t accumulator = 0;
    char c;
    bool trailing_data = false;
    while ((c = *verStr))
    {
        ++verStr;
        // A decimal indicates moving to a new version field
        if (c == '.')
        {
            retVal = (retVal << 8) | accumulator;
            accumulator = 0;
            trailing_data = false;
        }
        // Else if this is a number add it up
        else if (c >= '0' && c <= '9')
        {
            accumulator = (accumulator * 10) + (c - '0');
            trailing_data = true;
        }
        // Anything except [0-9.] ends the parsing
        else
        {
            break;
        }
    }
    if (trailing_data)
    {
        retVal = (retVal << 8) | accumulator;
    }
    // If the version ID is < 1.0.0, we could not parse it,
    // just use the OTA version as the major version number
    if (retVal < 0x010000)
    {
        retVal = OTA_VERSION_ID << 16;
    }
#endif
    return retVal;
}

void CRSF::GetDeviceInformation(uint8_t *frame, uint8_t fieldCount)
{
    const uint8_t size = strlen(device_name)+1;
    deviceInformationPacket_t *device = (deviceInformationPacket_t *)(frame + sizeof(crsf_ext_header_t) + size);
    // Packet starts with device name
    memcpy(frame + sizeof(crsf_ext_header_t), device_name, size);
    // Followed by the device
    device->serialNo = htobe32(0x454C5253); // ['E', 'L', 'R', 'S'], seen [0x00, 0x0a, 0xe7, 0xc6] // "Serial 177-714694" (value is 714694)
    device->hardwareVer = 0; // unused currently by us, seen [ 0x00, 0x0b, 0x10, 0x01 ] // "Hardware: V 1.01" / "Bootloader: V 3.06"
    device->softwareVer = htobe32(VersionStrToU32(version)); // seen [ 0x00, 0x00, 0x05, 0x0f ] // "Firmware: V 5.15"
    device->fieldCnt = fieldCount;
    device->parameterVersion = 0;
}

void CRSF::SetHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t destAddr)
{
    crsf_header_t *header = (crsf_header_t *)frame;
    header->device_addr = destAddr;
    header->frame_size = frameSize;
    header->type = frameType;

    uint8_t crc = crsf_crc.calc(&frame[CRSF_FRAME_NOT_COUNTED_BYTES], frameSize - 1, 0);
    frame[frameSize + CRSF_FRAME_NOT_COUNTED_BYTES - 1] = crc;
}

void CRSF::SetExtendedHeaderAndCrc(uint8_t *frame, uint8_t frameType, uint8_t frameSize, uint8_t senderAddr, uint8_t destAddr)
{
    crsf_ext_header_t *header = (crsf_ext_header_t *)frame;
    header->dest_addr = destAddr;
    header->orig_addr = senderAddr;
    SetHeaderAndCrc(frame, frameType, frameSize, destAddr);
}
