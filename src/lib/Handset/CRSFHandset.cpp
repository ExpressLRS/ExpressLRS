#if defined(TARGET_TX)
#include "CRSFHandset.h"

#include "CRSFEndpoint.h"
#include "CRSFRouter.h"
#include "FIFO.h"
#include "helpers.h"
#include "logging.h"

#if defined(PLATFORM_ESP32)
#include <hal/uart_ll.h>
#include <soc/soc.h>
#include <soc/uart_reg.h>
// UART0 is used since for DupleTX we can connect directly through IO_MUX and not the Matrix
// for better performance, and on other targets (mostly using pin 13), it always uses Matrix
HardwareSerial CRSFHandset::Port(0);
#elif defined(PLATFORM_ESP8266)
HardwareSerial CRSFHandset::Port(0);
#elif defined(TARGET_NATIVE)
HardwareSerial CRSFHandset::Port = Serial;
#endif

static constexpr int HANDSET_TELEMETRY_FIFO_SIZE = 128; // this is the smallest telemetry FIFO size in ETX with CRSF defined

/// Out FIFO to buffer messages///
static constexpr auto CRSF_SERIAL_OUT_FIFO_SIZE = 256U;
static FIFO<CRSF_SERIAL_OUT_FIFO_SIZE> SerialOutFIFO;

Stream *CRSFHandset::PortSecondary;

/// UART Handling ///
uint32_t CRSFHandset::GoodPktsCountResult = 0;
uint32_t CRSFHandset::BadPktsCountResult = 0;

bool CRSFHandset::halfDuplex = false;

/// OpenTX mixer sync ///
static constexpr int32_t OpenTXsyncPacketInterval = 200; // in ms
static constexpr int32_t OpenTXsyncOffsetSafeMargin = 1000; // 100us

/// UART Handling ///
static const int32_t TxToHandsetBauds[] = {400000, 115200, 5250000, 3750000, 1870000, 921600, 2250000};
uint8_t CRSFHandset::UARTcurrentBaudIdx = 6;   // only used for baud-cycling, initialized to the end so the next one we try is the first in the list
uint32_t CRSFHandset::UARTrequestedBaud = 5250000;

// for the UART wdt, every 1000ms we change bauds when connect is lost
static constexpr int UARTwdtInterval = 1000;

void CRSFHandset::Begin()
{
    DBGLN("About to start CRSF task...");

    addDevice(CRSF_ADDRESS_ELRS_LUA);
    addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(this);

    UARTwdtLastChecked = millis() + UARTwdtInterval; // allows a delay before the first time the UARTwdt() function is called

    halfDuplex = (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX);

#if defined(PLATFORM_ESP32)
    portDISABLE_INTERRUPTS();
    UARTinverted = halfDuplex; // on a full UART we will start uninverted checking first
    Port.begin(UARTrequestedBaud, SERIAL_8N1,
                     GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX,
                     false, 0);
    // Arduino defaults every esp32 stream to a 1000ms timeout which is just baffling
    Port.setTimeout(0);
    if (halfDuplex)
    {
        duplex_set_RX();
    }
    portENABLE_INTERRUPTS();
    flush_port_input();
#elif defined(PLATFORM_ESP8266)
    // Uses default UART pins
    Port.begin(UARTrequestedBaud);
    // Invert RX/TX (not done, connection is full duplex uninverted)
    //USC0(UART0) |= BIT(UCRXI) | BIT(UCTXI);
    // No log message because this is our only UART
#endif
}

void CRSFHandset::End()
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
    //Port.end(); // don't call serial.end(), it causes some sort of issue with the 900mhz hardware using gpio2 for serial
    DBGLN("CRSF UART END");
}

void CRSFHandset::flush_port_input()
{
    // Make sure there is no garbage on the UART at the start
    while (Port.available())
    {
        Port.read();
    }
}

void CRSFHandset::forwardMessage(const crsf_header_t *message)
{
    if (controllerConnected)
    {
        uint8_t size = CRSF_FRAME_SIZE(message->frame_size);
        if (size > CRSF_MAX_PACKET_LEN)
        {
            ERRLN("too large");
            return;
        }

        SerialOutFIFO.lock();
        if (SerialOutFIFO.ensure(size + 1))
        {
            auto data = (uint8_t *)message;
            // CRSF on a serial port _always_ has 0xC8 as a sync byte rather than the device_id.
            // See https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md#frame-details
            data[0] = CRSF_SYNC_BYTE;
            SerialOutFIFO.push(size); // length
            SerialOutFIFO.pushBytes(data, size);
        }
        SerialOutFIFO.unlock();
    }
}

void ICACHE_RAM_ATTR CRSFHandset::setPacketInterval(int32_t PacketInterval)
{
    RequestedRCpacketInterval = PacketInterval;
    OpenTXsyncOffset = 0;
    OpenTXsyncWindow = 0;
    OpenTXsyncWindowSize = std::max((int32_t)1, (int32_t)(20000/RequestedRCpacketInterval));
    OpenTXsyncLastSent -= OpenTXsyncPacketInterval;
    adjustMaxPacketSize();
}

void ICACHE_RAM_ATTR CRSFHandset::JustSentRFpacket()
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

void CRSFHandset::sendSyncPacketToTX() // in values in us.
{
    const uint32_t now = millis();
    if (now - OpenTXsyncLastSent >= OpenTXsyncPacketInterval)
    {
        int32_t packetRate = RequestedRCpacketInterval * 10; //convert from us to right format
        int32_t offset = OpenTXsyncOffset - OpenTXsyncOffsetSafeMargin; // offset so that opentx always has some headroom
#ifdef DEBUG_OPENTX_SYNC
        DBGLN("Offset %d", offset); // in 10ths of us (OpenTX sync unit)
#endif

        CRSF_MK_EXT_FRAME_T(crsf_sync_packet_t) sync_packet = {
            .h = {
                CRSF_ADDRESS_RADIO_TRANSMITTER,
                CRSF_EXT_FRAME_SIZE(sizeof(crsf_sync_packet_t)),
                CRSF_FRAMETYPE_HANDSET,
                CRSF_ADDRESS_RADIO_TRANSMITTER,
                CRSF_ADDRESS_CRSF_TRANSMITTER,
            },
            .p = {
                .subType = CRSF_HANDSET_SUBCMD_TIMING,
                .rate = htobe32(packetRate),
                .offset = htobe32(offset)
            },
            .crc = crsfRouter.crsf_crc.calc((uint8_t *)&sync_packet + CRSF_TELEMETRY_TYPE_INDEX, sizeof(sync_packet)-3)
        };
        crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, (crsf_header_t *)&sync_packet);

        OpenTXsyncLastSent = now;
    }
}

bool CRSFHandset::ProcessPacket()
{
    dataLastRecv = micros();

    if (!controllerConnected)
    {
        controllerConnected = true;
        DBGLN("CRSF UART Connected");
        if (connected) connected();
    }

    crsfRouter.processMessage(this, (crsf_header_t *)&inBuffer);

    return true;
}

void CRSFHandset::alignBufferToSync(uint8_t startIdx)
{
    for (unsigned int i=startIdx ; i<SerialInPacketPtr ; i++)
    {
        // If we find a header byte then move that and trailing bytes to the head of the buffer and let's go!
        if (inBuffer[i] == CRSF_ADDRESS_CRSF_TRANSMITTER || inBuffer[i] == CRSF_SYNC_BYTE)
        {
            SerialInPacketPtr -= i;
            memmove(inBuffer, &inBuffer[i], SerialInPacketPtr);
            return;
        }
    }

    // If no header found then discard this entire buffer
    SerialInPacketPtr = 0;
}

void CRSFHandset::handleInput()
{
    if (UARTwdt())
    {
        return;
    }

    if (transmitting)
    {
        // if currently transmitting in half-duplex mode then check if the TX buffers are empty.
        // If there is still data in the transmit buffers then exit, and we'll check next go round.
#if defined(PLATFORM_ESP32)
        if (!uart_ll_is_tx_idle(UART_LL_GET_HW(0)))
        {
            return;
        }
#elif defined(PLATFORM_ESP8266)
        if (((USS(0) >> USTXC) & 0xff) > 0)
        {
            return;
        }
#endif
        // All done transmitting; go back to receive mode
        transmitting = false;
        duplex_set_RX();
        flush_port_input();
    }

    // Add new data, and then discard bytes until we start with header byte
    auto toRead = std::min(Port.available(), CRSF_MAX_PACKET_LEN - SerialInPacketPtr);
    SerialInPacketPtr += Port.readBytes(&inBuffer[SerialInPacketPtr], toRead);
    alignBufferToSync(0);

    // Make sure we have at least a packet header and a length byte
    if (SerialInPacketPtr < 3)
        return;

    // Sanity check: A total packet must be at least [sync][len][type][crc] (if no payload) and at most CRSF_MAX_PACKET_LEN
    const uint32_t totalLen = inBuffer[1] + 2;
    if (totalLen < 4 || totalLen > CRSF_MAX_PACKET_LEN)
    {
        // Start looking for another packet after this start byte
        alignBufferToSync(1);
        return;
    }

    // Only proceed one there are enough bytes in the buffer for the entire packet
    if (SerialInPacketPtr < totalLen)
        return;

    uint8_t CalculatedCRC = crsfRouter.crsf_crc.calc(&inBuffer[2], totalLen - 3);
    if (CalculatedCRC == inBuffer[totalLen - 1])
    {
        GoodPktsCount++;
        ProcessPacket();
        handleOutput(totalLen);
    }
    else
    {
        DBGLN("UART CRC failure");
        BadPktsCount++;
    }

    SerialInPacketPtr -= totalLen;
    memmove(inBuffer, &inBuffer[totalLen], SerialInPacketPtr);
}

void CRSFHandset::handleOutput(const uint32_t receivedBytes)
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
        uint8_t periodBytesRemaining = HANDSET_TELEMETRY_FIFO_SIZE;
        if (halfDuplex)
        {
            periodBytesRemaining = std::min(maxPeriodBytes - receivedBytes % maxPeriodBytes, (uint32_t)maxPacketBytes);
            periodBytesRemaining = std::max(periodBytesRemaining, (uint8_t)10);
            if (!transmitting)
            {
                transmitting = true;
                duplex_set_TX();
            }
        }

        do
        {
            SerialOutFIFO.lock();
            // no package is in transit so get new data from the fifo
            if (packageLengthRemaining == 0)
            {
                packageLengthRemaining = SerialOutFIFO.pop();
                SerialOutFIFO.popBytes(CRSFoutBuffer, packageLengthRemaining);
                sendingOffset = 0;
            }
            SerialOutFIFO.unlock();

            // if the package is long we need to split it, so it fits in the sending interval
            uint8_t writeLength = std::min(packageLengthRemaining, periodBytesRemaining);

            // write the packet out, if it's a large package the offset holds the starting position
            Port.write(CRSFoutBuffer + sendingOffset, writeLength);
            if (PortSecondary)
            {
                PortSecondary->write(CRSFoutBuffer + sendingOffset, writeLength);
            }

            sendingOffset += writeLength;
            packageLengthRemaining -= writeLength;
            periodBytesRemaining -= writeLength;
        } while(periodBytesRemaining != 0 && SerialOutFIFO.size() != 0);
    }
}

void CRSFHandset::duplex_set_RX() const
{
#if defined(PLATFORM_ESP32)
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
#elif defined(PLATFORM_ESP8266)
    // Enable loopback on UART0 to connect the RX pin to the TX pin (not done, connection is full duplex uninverted)
    //USC0(UART0) |= BIT(UCLBE);
#endif
}

void CRSFHandset::duplex_set_TX() const
{
#if defined(PLATFORM_ESP32)
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
#elif defined(PLATFORM_ESP8266)
    // Disable loopback to disconnect the RX pin from the TX pin (not done, connection is full duplex uninverted)
    //USC0(UART0) &= ~BIT(UCLBE);
#endif
}

int CRSFHandset::getMinPacketInterval() const
{
    if (halfDuplex && UARTrequestedBaud == 115200) // Packet rate limited to 200Hz if we are on 115k baud on half-duplex module
    {
        return 5000;
    }
    if (UARTrequestedBaud == 115200) // Packet rate limited to 250Hz if we are on 115k baud
    {
        return 4000;
    }
    if (UARTrequestedBaud == 400000) // Packet rate limited to 500Hz if we are on 400k baud
    {
        return 2000;
    }
    return 1;   // 1-million Hz!
}

void ICACHE_RAM_ATTR CRSFHandset::adjustMaxPacketSize()
{
    const int LUA_CHUNK_QUERY_SIZE = 26;
    // The number of bytes that fit into a CRSF window : baud / 10bits-per-byte / rate(Hz) * 87% (for some leeway)
    // 87% was arrived at by measuring the time taken for a chunk query packet and the processing times and switching times
    // involved from RX -> TX and vice-versa. The maxPacketBytes is used as the Lua chunk size so each chunk can be returned
    // to the handset and not be broken across time-slots as there can be issues with spurious glitches on the s.port pin
    // which switching direction. It also appears that the absolute minimum packet size should be 15 bytes as this will fit
    // the LinkStatistics and OpenTX sync packets.
    maxPeriodBytes = std::min((int)(UARTrequestedBaud / 10 / (1000000/RequestedRCpacketInterval) * 87 / 100), HANDSET_TELEMETRY_FIFO_SIZE);
    // Maximum number of bytes we can send in a single window, half the period bytes, upto one full CRSF packet.
    maxPacketBytes = std::min(maxPeriodBytes - max(maxPeriodBytes / 2, LUA_CHUNK_QUERY_SIZE), CRSF_MAX_PACKET_LEN);
    DBGLN("Adjusted max packet size %u-%u", maxPacketBytes, maxPeriodBytes);
}

#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
uint32_t CRSFHandset::autobaud()
{
    static enum { INIT, MEASURED, INVERTED } state;

    if (state == MEASURED)
    {
        UARTinverted = !UARTinverted;
        state = INVERTED;
        return UARTrequestedBaud;
    }
    if (state == INVERTED)
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
    if (REG_GET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN) && REG_READ(UART_RXD_CNT_REG(0)) < 300)
    {
        return 400000;
    }

    state = MEASURED;

    const uint32_t low_period  = REG_READ(UART_LOWPULSE_REG(0));
    const uint32_t high_period = REG_READ(UART_HIGHPULSE_REG(0));
    REG_CLR_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN); // disable autobaud
    REG_CLR_BIT(UART_RX_FILT_REG(0), UART_GLITCH_FILT_EN); // disable glitch filtering

    DBGLN("autobaud: low %d, high %d", low_period, high_period);
    // According to the tecnnical reference
    const int32_t calulatedBaud = UART_CLK_FREQ / (low_period + high_period + 2);
    int32_t bestBaud = TxToHandsetBauds[0];
    for(int i=0 ; i<ARRAY_SIZE(TxToHandsetBauds) ; i++)
    {
        if (abs(calulatedBaud - bestBaud) > abs(calulatedBaud - TxToHandsetBauds[i]))
        {
            bestBaud = TxToHandsetBauds[i];
        }
    }
    return bestBaud;
}
#elif defined(PLATFORM_ESP32)
uint32_t CRSFHandset::autobaud()
{
    static enum { INIT, MEASURED, INVERTED } state;

    if (state == MEASURED) {
        UARTinverted = !UARTinverted;
        state = INVERTED;
        return UARTrequestedBaud;
    }
    if (state == INVERTED) {
        UARTinverted = !UARTinverted;
        state = INIT;
    }

    if (REG_GET_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN) == 0) {
        REG_WRITE(UART_AUTOBAUD_REG(0), 4 << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN);    // enable, glitch filter 4
        return 400000;
    }
    if (REG_GET_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN) && REG_READ(UART_RXD_CNT_REG(0)) < 300)
    {
        return 400000;
    }

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
uint32_t CRSFHandset::autobaud() {
    UARTcurrentBaudIdx = (UARTcurrentBaudIdx + 1) % ARRAY_SIZE(TxToHandsetBauds);
    return TxToHandsetBauds[UARTcurrentBaudIdx];
}
#endif

bool CRSFHandset::UARTwdt()
{
    bool retval = false;
#if !defined(DEBUG_TX_FREERUN)
    uint32_t now = millis();
    if (now - UARTwdtLastChecked > UARTwdtInterval)
    {
        // If no packets or more bad than good packets, rate cycle/autobaud the UART but
        // do not adjust the parameters while in wifi mode. If a firmware is being
        // uploaded, it will cause tons of serial errors during the flash writes
        if ((connectionState != wifiUpdate) && (BadPktsCount >= GoodPktsCount || !controllerConnected))
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
                Port.flush();
                Port.updateBaudRate(UARTrequestedBaud);
                if (halfDuplex)
                {
                    duplex_set_RX();
                }
                // cleanup input buffer
                flush_port_input();
            }
            retval = true;
        }
#ifdef DEBUG_OPENTX_SYNC
        if (abs((int)((1000000 / (ExpressLRS_currAirRate_Modparams->interval * ExpressLRS_currAirRate_Modparams->numOfSends)) - (int)GoodPktsCount)) > 1)
#endif
            DBGVLN("UART STATS Bad:Good = %u:%u", BadPktsCount, GoodPktsCount);

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
#endif
