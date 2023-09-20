#include "CRSF.h"
#include "CRSFController.h"
#include "FIFO.h"
#include "logging.h"
#include "helpers.h"

#if defined(CRSF_TX_MODULE) && !defined(UNIT_TEST)
#include "device.h"

#if defined(PLATFORM_ESP32)
#include <soc/uart_reg.h>
// UART0 is used since for DupleTX we can connect directly through IO_MUX and not the Matrix
// for better performance, and on other targets (mostly using pin 13), it always uses Matrix
HardwareSerial CRSFController::Port(0);
RTC_DATA_ATTR int rtcModelId = 0;
#elif defined(PLATFORM_ESP8266)
HardwareSerial CRSFController::Port(0);
#elif defined(PLATFORM_STM32)
HardwareSerial CRSFController::Port(GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
#if defined(STM32F3) || defined(STM32F3xx)
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#elif defined(STM32F1) || defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#endif
#elif defined(TARGET_NATIVE)
HardwareSerial CRSFController::Port = Serial;
#endif

static const int HANDSET_TELEMETRY_FIFO_SIZE = 128; // this is the smallest telemetry FIFO size in ETX with CRSF defined

/// Out FIFO to buffer messages///
static const auto CRSF_SERIAL_OUT_FIFO_SIZE = 256U;
static FIFO<CRSF_SERIAL_OUT_FIFO_SIZE> SerialOutFIFO;

Stream *CRSFController::PortSecondary;

/// UART Handling ///
uint32_t CRSFController::GoodPktsCountResult = 0;
uint32_t CRSFController::BadPktsCountResult = 0;

uint8_t CRSFController::modelId = 0;
bool CRSFController::ForwardDevicePings = false;
bool CRSFController::elrsLUAmode = false;

/// OpenTX mixer sync ///
static const int32_t OpenTXsyncPacketInterval = 200; // in ms
static const int32_t OpenTXsyncOffsetSafeMargin = 1000; // 100us

/// UART Handling ///
static const int32_t TxToHandsetBauds[] = {400000, 115200, 5250000, 3750000, 1870000, 921600, 2250000};
uint8_t CRSFController::UARTcurrentBaudIdx = 6;   // only used for baud-cycling, initialized to the end so the next one we try is the first in the list
uint32_t CRSFController::UARTrequestedBaud = 5250000;

// for the UART wdt, every 1000ms we change bauds when connect is lost
static const int UARTwdtInterval = 1000;

void CRSFController::Begin()
{
    DBGLN("About to start CRSF task...");

    UARTwdtLastChecked = millis() + UARTwdtInterval; // allows a delay before the first time the UARTwdt() function is called

#if defined(PLATFORM_ESP32)
    portDISABLE_INTERRUPTS();
    if (GPIO_PIN_RCSIGNAL_RX != GPIO_PIN_RCSIGNAL_TX)
    {
        UARTinverted = false; // on a full UART we will start uninverted checking first
    }
    CRSFController::Port.begin(UARTrequestedBaud, SERIAL_8N1,
                     GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX,
                     false, 500);
    CRSFController::duplex_set_RX();
    portENABLE_INTERRUPTS();
    flush_port_input();
    if (esp_reset_reason() != ESP_RST_POWERON)
    {
        modelId = rtcModelId;
        if (RecvModelUpdate) RecvModelUpdate();
    }
#elif defined(PLATFORM_ESP8266)
    // Uses default UART pins
    CRSFController::Port.begin(UARTrequestedBaud);
    // Invert RX/TX (not done, connection is full duplex uninverted)
    //USC0(UART0) |= BIT(UCRXI) | BIT(UCTXI);
    // No log message because this is our only UART

#elif defined(PLATFORM_STM32)
    DBGLN("Start STM32 R9M TX CRSF UART");

    CRSFController::Port.setTx(GPIO_PIN_RCSIGNAL_TX);
    CRSFController::Port.setRx(GPIO_PIN_RCSIGNAL_RX);

    #if defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    pinMode(GPIO_PIN_BUFFER_OE, OUTPUT);
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED); // RX mode default
    #elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    CRSFController::Port.setHalfDuplex();
    #endif

    CRSFController::Port.begin(UARTrequestedBaud);

#if defined(TARGET_TX_GHOST)
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR3 |= USART_CR3_HDSEL;
    USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
    USART1->CR1 |= USART_CR1_UE;
#elif defined(TARGET_TX_FM30_MINI)
    LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
    USART2->CR1 &= ~USART_CR1_UE;
    USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
    USART2->CR1 |= USART_CR1_UE;
#endif
    DBGLN("STM32 CRSF UART LISTEN TASK STARTED");
    CRSFController::Port.flush();
    flush_port_input();
#endif
}

void CRSFController::End()
{
    uint32_t startTime = millis();
    while (SerialOutFIFO.peek() > 0)
    {
        handleInput();
        if (millis() - startTime > 1000)
        {
            break;
        }
    }
    //CRSFController::Port.end(); // don't call serial.end(), it causes some sort of issue with the 900mhz hardware using gpio2 for serial
    DBGLN("CRSF UART END");
}

void CRSFController::flush_port_input()
{
    // Make sure there is no garbage on the UART at the start
    while (CRSFController::Port.available())
    {
        CRSFController::Port.read();
    }
}

void CRSFController::makeLinkStatisticsPacket(uint8_t buffer[LinkStatisticsFrameLength + 4])
{
    buffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buffer[1] = LinkStatisticsFrameLength + 2;
    buffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;
    for (uint8_t i = 0; i < LinkStatisticsFrameLength; i++)
    {
        buffer[i + 3] = ((uint8_t *)&CRSF::LinkStatistics)[i];
    }
    uint8_t crc = crsf_crc.calc(buffer[2]);
    buffer[LinkStatisticsFrameLength + 3] = crsf_crc.calc((byte *)&CRSF::LinkStatistics, LinkStatisticsFrameLength, crc);
}

/**
 * Build a an extended type packet and queue it in the SerialOutFIFO
 * This is just a regular packet with 2 extra bytes with the sub src and target
 **/
void CRSFController::packetQueueExtended(uint8_t type, void *data, uint8_t len)
{
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

    SerialOutFIFO.lock();
    if (SerialOutFIFO.ensure(buf[0] + 1))
    {
        SerialOutFIFO.pushBytes(buf, sizeof(buf));
        SerialOutFIFO.pushBytes((byte *)data, len);
        SerialOutFIFO.push(crc);
    }
    SerialOutFIFO.unlock();
}

void CRSFController::sendTelemetryToTX(uint8_t *data)
{
    if (controllerConnected)
    {
        uint8_t size = CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX]);
        if (size > CRSF_MAX_PACKET_LEN)
        {
            ERRLN("too large");
            return;
        }

        data[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
        SerialOutFIFO.lock();
        if (SerialOutFIFO.ensure(size + 1))
        {
            SerialOutFIFO.push(size); // length
            SerialOutFIFO.pushBytes(data, size);
        }
        SerialOutFIFO.unlock();
    }
}

void ICACHE_RAM_ATTR CRSFController::setPacketInterval(int32_t PacketInterval)
{
    RequestedRCpacketInterval = PacketInterval;
    OpenTXsyncOffset = 0;
    OpenTXsyncWindow = 0;
    OpenTXsyncWindowSize = std::max((int32_t)1, (int32_t)(20000/RequestedRCpacketInterval));
    OpenTXsyncLastSent -= OpenTXsyncPacketInterval;
    adjustMaxPacketSize();
}

void ICACHE_RAM_ATTR CRSFController::JustSentRFpacket()
{
    // read them in this order to prevent a potential race condition
    uint32_t last = dataLastRecv;
    uint32_t m = micros();
    auto delta = (int32_t)(m - last);

    if (delta >= (int32_t)RequestedRCpacketInterval)
    {
        // missing/late packet, force resync
        OpenTXsyncOffset = -(delta % RequestedRCpacketInterval) * 10;
        OpenTXsyncWindow = 0;
        OpenTXsyncLastSent -= OpenTXsyncPacketInterval;
#ifdef DEBUG_OPENTX_SYNC
        DBGLN("Missed packet, forced resync (%d)!", delta);
#endif
    }
    else
    {
        // The number of packets in the sync window is how many will fit in 20ms.
        // This gives quite quite coarse changes for 50Hz, but more fine grained changes at 1000Hz.
        OpenTXsyncWindow = std::min(OpenTXsyncWindow + 1, (int32_t)OpenTXsyncWindowSize);
        OpenTXsyncOffset = ((OpenTXsyncOffset * (OpenTXsyncWindow-1)) + delta * 10) / OpenTXsyncWindow;
    }
}

void CRSFController::sendSyncPacketToTX() // in values in us.
{
    uint32_t now = millis();
    if (controllerConnected && (now - OpenTXsyncLastSent) >= OpenTXsyncPacketInterval)
    {
        int32_t packetRate = RequestedRCpacketInterval * 10; //convert from us to right format
        int32_t offset = OpenTXsyncOffset - OpenTXsyncOffsetSafeMargin; // offset so that opentx always has some headroom
#ifdef DEBUG_OPENTX_SYNC
        DBGLN("Offset %d", offset); // in 10ths of us (OpenTX sync unit)
#endif

        struct otxSyncData {
            uint8_t extendedType; // CRSF_FRAMETYPE_OPENTX_SYNC
            uint32_t rate; // Big-Endian
            uint32_t offset; // Big-Endian
        } PACKED;

        uint8_t buffer[sizeof(otxSyncData)];
        auto * const sync = (struct otxSyncData * const)buffer;

        sync->extendedType = CRSF_FRAMETYPE_OPENTX_SYNC;
        sync->rate = htobe32(packetRate);
        sync->offset = htobe32(offset);

        packetQueueExtended(CRSF_FRAMETYPE_RADIO_ID, buffer, sizeof(buffer));

        OpenTXsyncLastSent = now;
    }
}

void CRSFController::RcPacketToChannelsData() // data is packed as 11 bits per channel
{
    // for monitoring arming state
    uint32_t prev_AUX1 = ChannelData[4];

    auto payload = (uint8_t const * const)&inBuffer.asRCPacket_t.channels;
    constexpr unsigned srcBits = 11;
    constexpr unsigned dstBits = 11;
    constexpr unsigned inputChannelMask = (1 << srcBits) - 1;
    constexpr unsigned precisionShift = dstBits - srcBits;

    // code from BetaFlight rx/crsf.cpp / bitpacker_unpack
    uint8_t bitsMerged = 0;
    uint32_t readValue = 0;
    unsigned readByteIndex = 0;
    for (uint32_t & n : ChannelData)
    {
        while (bitsMerged < srcBits)
        {
            uint8_t readByte = payload[readByteIndex++];
            readValue |= ((uint32_t) readByte) << bitsMerged;
            bitsMerged += 8;
        }
        //printf("rv=%x(%x) bm=%u\n", readValue, (readValue & inputChannelMask), bitsMerged);
        n = (readValue & inputChannelMask) << precisionShift;
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

bool CRSFController::ProcessPacket()
{
    bool packetReceived = false;

    CRSFController::dataLastRecv = micros();

    if (!controllerConnected)
    {
        controllerConnected = true;
        DBGLN("CRSF UART Connected");
        if (connected) connected();
    }

    const uint8_t packetType = inBuffer.asRCPacket_t.header.type;
    uint8_t *SerialInBuffer = inBuffer.asUint8_t;

    if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        RCdataLastRecv = micros();
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
            const uint8_t length = inBuffer.asRCPacket_t.header.frame_size + 2;
            CRSF::AddMspMessage(length, SerialInBuffer);
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

void CRSFController::handleInput()
{
    uint8_t *SerialInBuffer = inBuffer.asUint8_t;

    if (UARTwdt())
    {
        return;
    }

    while (CRSFController::Port.available())
    {
        if (!CRSFframeActive)
        {
            unsigned char const inChar = CRSFController::Port.read();
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
                unsigned char const inChar = CRSFController::Port.read();
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
            auto count = (int)CRSFController::Port.read(&SerialInBuffer[SerialInPacketPtr], toRead);
            #else
            int count = 0;
            auto avail = (int)CRSFController::Port.available();
            while (count < toRead && count < avail)
            {
                SerialInBuffer[SerialInPacketPtr + count] = CRSFController::Port.read();
                count++;
            }
            #endif
            SerialInPacketPtr += count;

            if (SerialInPacketPtr >= (SerialInPacketLen + 2)) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
            {
                uint8_t CalculatedCRC = crsf_crc.calc(SerialInBuffer + 2, SerialInPacketPtr - 3);

                if (CalculatedCRC == SerialInBuffer[SerialInPacketPtr-1])
                {
                    GoodPktsCount++;
                    if (ProcessPacket())
                    {
                        //delayMicroseconds(50);
                        handleOutput();
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

void CRSFController::handleOutput()
{
    static uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN] = {0};
    // both static to split up larger packages
    static uint8_t packageLengthRemaining = 0;
    static uint8_t sendingOffset = 0;

    if (!controllerConnected)
    {
        SerialOutFIFO.lock();
        SerialOutFIFO.flush();
        SerialOutFIFO.unlock();
        return;
    }

    if (packageLengthRemaining == 0 && SerialOutFIFO.size() == 0)
    {
        sendSyncPacketToTX(); // calculate mixer sync packet if needed
    }

    // if partial package remaining, or data in the output FIFO that needs to be written
    if (packageLengthRemaining > 0 || SerialOutFIFO.size() > 0) {
        duplex_set_TX();

        uint8_t periodBytesRemaining = maxPeriodBytes;
        while (periodBytesRemaining)
        {
            SerialOutFIFO.lock();
            // no package is in transit so get new data from the fifo
            if (packageLengthRemaining == 0) {
                packageLengthRemaining = SerialOutFIFO.pop();
                SerialOutFIFO.popBytes(CRSFoutBuffer, packageLengthRemaining);
                sendingOffset = 0;
            }
            SerialOutFIFO.unlock();

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
            CRSFController::Port.write(CRSFoutBuffer + sendingOffset, writeLength);
            if (CRSFController::PortSecondary)
                CRSFController::PortSecondary->write(CRSFoutBuffer + sendingOffset, writeLength);

            sendingOffset += writeLength;
            packageLengthRemaining -= writeLength;
            periodBytesRemaining -= writeLength;

            // No bytes left to send, exit
            if (SerialOutFIFO.size() == 0)
                break;
        }
        CRSFController::Port.flush();
        duplex_set_RX();

        // make sure there is no garbage on the UART left over
        flush_port_input();
    }
}

void CRSFController::duplex_set_RX() const
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
    CRSFController::Port.enableHalfDuplexRx();
#endif
}

void CRSFController::duplex_set_TX() const
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

void ICACHE_RAM_ATTR CRSFController::adjustMaxPacketSize()
{
    // baud / 10bits-per-byte / 2 windows (1RX, 1TX) / rate * 0.80 (leeway)
    maxPeriodBytes = UARTrequestedBaud / 10 / 2 / (1000000/RequestedRCpacketInterval) * 80 / 100;
    maxPeriodBytes = maxPeriodBytes > HANDSET_TELEMETRY_FIFO_SIZE ? HANDSET_TELEMETRY_FIFO_SIZE : maxPeriodBytes;
    // we need a minimum of 10 bytes otherwise our LUA will not make progress and at 8 we'd get a divide by 0!
    maxPeriodBytes = maxPeriodBytes < 10 ? 10 : maxPeriodBytes;
    maxPacketBytes = maxPeriodBytes > CRSF_MAX_PACKET_LEN ? CRSF_MAX_PACKET_LEN : maxPeriodBytes;
    DBGLN("Adjusted max packet size %u-%u", maxPacketBytes, maxPeriodBytes);
}

#if defined(PLATFORM_ESP32_S3)
uint32_t CRSFController::autobaud()
{
    static enum { INIT, MEASURED, INVERTED } state;

    if (state == MEASURED)
    {
        UARTinverted = !UARTinverted;
        state = INVERTED;
        return UARTrequestedBaud;
    }
    else if (state == INVERTED)
    {
        UARTinverted = !UARTinverted;
        state = INIT;
    }

    if (REG_GET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN) == 0)
    {
        REG_WRITE(UART_RX_FILT_REG(0), (4 << UART_GLITCH_FILT_S) | UART_GLITCH_FILT_EN); // enable, glitch filter 4
        REG_WRITE(UART_LOWPULSE_REG(0), 4095); // reset register to max value
        REG_WRITE(UART_HIGHPULSE_REG(0), 4095); // reset register to max value
        REG_SET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN); // enable autobaud
        return 400000;
    }
    else if (REG_GET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN) && REG_READ(UART_RXD_CNT_REG(0)) < 300)
    {
        return 400000;
    }

    state = MEASURED;

    uint32_t low_period  = REG_READ(UART_LOWPULSE_REG(0));
    uint32_t high_period = REG_READ(UART_HIGHPULSE_REG(0));
    REG_CLR_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN); // disable autobaud
    REG_CLR_BIT(UART_RX_FILT_REG(0), UART_GLITCH_FILT_EN); // disable glitch filtering

    DBGLN("autobaud: low %d, high %d", low_period, high_period);
    // According to the tecnnical reference
    int32_t calulatedBaud = UART_CLK_FREQ / (low_period + high_period + 2);
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
#elif defined(PLATFORM_ESP32)
uint32_t CRSFController::autobaud()
{
    static enum { INIT, MEASURED, INVERTED } state;

    if (state == MEASURED) {
        UARTinverted = !UARTinverted;
        state = INVERTED;
        return UARTrequestedBaud;
    } else if (state == INVERTED) {
        UARTinverted = !UARTinverted;
        state = INIT;
    }

    if (REG_GET_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN) == 0) {
        REG_WRITE(UART_AUTOBAUD_REG(0), 4 << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN);    // enable, glitch filter 4
        return 400000;
    } else if (REG_GET_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN) && REG_READ(UART_RXD_CNT_REG(0)) < 300)
        return 400000;

    state = MEASURED;

    auto low_period  = (int32_t)REG_READ(UART_LOWPULSE_REG(0));
    auto high_period = (int32_t)REG_READ(UART_HIGHPULSE_REG(0));
    REG_CLR_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN);   // disable autobaud

    DBGLN("autobaud: low %d, high %d", low_period, high_period);
    // sample code at https://github.com/espressif/esp-idf/issues/3336
    // says baud rate = 80000000/min(UART_LOWPULSE_REG, UART_HIGHPULSE_REG);
    // Based on testing use max and add 2 for lowest deviation
    int32_t calculatedBaud = 80000000 / (max(low_period, high_period) + 3);
    auto bestBaud = TxToHandsetBauds[0];
    for(int TxToHandsetBaud : TxToHandsetBauds)
    {
        if (abs(calculatedBaud - bestBaud) > abs(calculatedBaud - (int32_t)TxToHandsetBaud))
        {
            bestBaud = (int32_t)TxToHandsetBaud;
        }
    }
    return bestBaud;
}
#else
uint32_t CRSFController::autobaud() {
    UARTcurrentBaudIdx = (UARTcurrentBaudIdx + 1) % ARRAY_SIZE(TxToHandsetBauds);
    return TxToHandsetBauds[UARTcurrentBaudIdx];
}
#endif

bool CRSFController::UARTwdt()
{
    bool retval = false;
#if !defined(DEBUG_TX_FREERUN)
    uint32_t now = millis();
    if (now >= (UARTwdtLastChecked + UARTwdtInterval))
    {
        if (BadPktsCount >= GoodPktsCount || !controllerConnected)
        {
            DBGLN("Too many bad UART RX packets!");

            if (controllerConnected)
            {
                DBGLN("CRSF UART Disconnected");
                if (disconnected) disconnected();
                controllerConnected = false;
            }

            UARTrequestedBaud = autobaud();
            if (UARTrequestedBaud != 0)
            {
                DBGLN("UART WDT: Switch to: %d baud", UARTrequestedBaud);

                adjustMaxPacketSize();

                SerialOutFIFO.flush();
#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
                CRSFController::Port.flush();
                CRSFController::Port.updateBaudRate(UARTrequestedBaud);
#elif defined(TARGET_TX_GHOST)
                CRSFController::Port.begin(UARTrequestedBaud);
                USART1->CR1 &= ~USART_CR1_UE;
                USART1->CR3 |= USART_CR3_HDSEL;
                USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
                USART1->CR1 |= USART_CR1_UE;
#elif defined(TARGET_TX_FM30_MINI)
                CRSFController::Port.begin(UARTrequestedBaud);
                LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
                USART2->CR1 &= ~USART_CR1_UE;
                USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
                USART2->CR1 |= USART_CR1_UE;
#else
                CRSFController::Port.begin(UARTrequestedBaud);
#endif
                duplex_set_RX();
                // cleanup input buffer
                flush_port_input();
            }
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

#endif // CRSF_TX_MODULE
