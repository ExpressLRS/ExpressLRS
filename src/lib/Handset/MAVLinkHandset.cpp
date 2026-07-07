// Always compiled on TX; AutoDetect spawns this handset at runtime when it detects MAVLink (no build flag).
#if defined(TARGET_TX)

#include "targets.h"

#include "MAVLinkHandset.h"
#include "CRSFHandset.h"   // reuse the UART0 HardwareSerial
#include "crsf_protocol.h"
#include "CRSFRouter.h"
#include "common.h"
#include "logging.h"
#include "OTA.h"

#if defined(PLATFORM_ESP32)
#include <hal/uart_ll.h>
#endif

#include <algorithm>

#define MAVLINK_COMM_NUM_BUFFERS 1
#include "common/mavlink.h"

static_assert(MAV_MAX_FRAME_LEN == MAVLINK_MAX_PACKET_LEN, "outBuf/inFrameBuf size drifted from the MAVLink dialect");

// How long without a valid RC frame before we declare the handset disconnected
static constexpr uint32_t MAVLINK_HANDSET_TIMEOUT_MS = 1000;

// Alternate RX polarity until a frame decodes (line can arrive inverted)
static constexpr uint32_t POLARITY_PROBE_INTERVAL_MS = 500;

// Down this long before forgetting a latched baud; holding it through brief drops stops the OTA rate dropping to 50Hz
static constexpr uint32_t BAUD_REACQUIRE_AFTER_MS = 3000;

// If neither polarity frames within this window the autobaud measurement was likely off (noise) - re-measure
static constexpr uint32_t BAUD_REMEASURE_AFTER_MS = 1500;

// Steady keepalive cadence (5Hz)
static constexpr uint32_t RADIO_STATUS_INTERVAL_MS = 200;

// Synthetic HEARTBEAT cadence (1Hz)
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 1000;

// Suppress the synthetic HEARTBEAT while a real one was seen this recently
static constexpr uint32_t FC_HEARTBEAT_PRESENT_MS = 2000;

// The ceiling on the chunk budget
static constexpr uint8_t HANDSET_TELEMETRY_FIFO_SIZE = 128;

// Tags ELRS CRSF param frames tunneled in MAVLink TUNNEL; must byte-match EdgeTX's 0x454C ('EL')
static constexpr uint16_t MAV_TUNNEL_PAYLOAD_TYPE_ELRS_CRSF = 0x454C;

// MAVLink framing constants, used to find FC frame boundaries so a keepalive is only inserted between whole frames.
static constexpr uint8_t MAVLINK_STX_V2 = 0xFD; // MAVLink2 start-of-frame
static constexpr uint8_t MAVLINK_STX_V1 = 0xFE; // MAVLink1 start-of-frame
static constexpr uint8_t MAVLINK_V2_HEADER_LEN = 10; // STX,LEN,incompat,compat,seq,sysid,compid,msgid[3]
static constexpr uint8_t MAVLINK_V1_HEADER_LEN = 6;  // STX,LEN,seq,sysid,compid,msgid
static constexpr uint8_t MAVLINK_CHECKSUM_LEN = 2;
static constexpr uint8_t MAVLINK_V2_SIGNATURE_LEN = 13; // present iff incompat_flags & MAVLINK_IFLAG_SIGNED

// ETXSYNC emit cadence
static constexpr uint32_t SYNC_PACKET_INTERVAL_MS = 100;

// Candidate handset bauds
static constexpr uint32_t MAVLINK_HANDSET_BAUDS[] = {115200, 400000, 921600};

#if defined(PLATFORM_ESP32)
void MAVLinkHandset::setRxPolarity(bool inverted) const
{
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
    if (inverted)
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

void MAVLinkHandset::duplexSetTX() const
{
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
    if (rxInverted)
    {
        ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0));
        ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
        constexpr uint8_t MATRIX_DETACH_IN_LOW = 0x30;
        gpio_matrix_in(MATRIX_DETACH_IN_LOW, U0RXD_IN_IDX, false);
        gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U0TXD_OUT_IDX, true, false);
    }
    else
    {
        ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 1));
        ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
        constexpr uint8_t MATRIX_DETACH_IN_HIGH = 0x38;
        gpio_matrix_in(MATRIX_DETACH_IN_HIGH, U0RXD_IN_IDX, false);
        gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U0TXD_OUT_IDX, false, false);
    }
}
#endif

void MAVLinkHandset::Begin()
{
    DBGLN("Starting MAVLink handset @ %u baud (autobaud)", (unsigned)currentBaud);

    addDevice(CRSF_ADDRESS_ELRS_LUA);
    addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(this);

    halfDuplex = (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX);

    setPacketInterval(RequestedRCpacketInterval);

#if defined(PLATFORM_ESP32)
    CRSFHandset::Port.begin(currentBaud, SERIAL_8N1, GPIO_PIN_RCSIGNAL_RX, halfDuplex ? -1 : GPIO_PIN_RCSIGNAL_TX, false, 0);
    CRSFHandset::Port.setTimeout(0);
    setRxPolarity(rxInverted);
    lastPolarityProbeMs = millis();
#elif defined(PLATFORM_ESP8266)
    CRSFHandset::Port.begin(currentBaud);
    CRSFHandset::Port.setTimeout(0);
#endif
}

void MAVLinkHandset::End()
{
}

MAVLinkHandset::~MAVLinkHandset()
{
    crsfRouter.removeConnector(this);
}

void MAVLinkHandset::computeChunkBudget(uint32_t periodUs)
{
    uint32_t periodBytes = (uint64_t)currentBaud * periodUs / 10000000ULL * 87 / 100;
    if (periodBytes > HANDSET_TELEMETRY_FIFO_SIZE)
        periodBytes = HANDSET_TELEMETRY_FIFO_SIZE;
    int32_t pkt = (int32_t)periodBytes - std::max((int32_t)(periodBytes / 2), (int32_t)26);
    maxPacketBytes = (uint8_t)std::max(pkt, (int32_t)16);
}

uint32_t MAVLinkHandset::edgetxRcPeriodUs() const
{
    return (currentBaud >= 921600) ? 2000 :
           (currentBaud >= 400000) ? 4000 : 16000;
}

void MAVLinkHandset::setBaud(uint32_t baud)
{
    if (baud == currentBaud)
        return;
    currentBaud = baud;
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    CRSFHandset::Port.updateBaudRate(baud);
#endif
    setPacketInterval(RequestedRCpacketInterval);
    DBGLN("MAVLink handset probing %u baud", (unsigned)baud);
}

uint32_t MAVLinkHandset::hwAutobaud()
{
#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
    if (REG_GET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN) == 0)
    {
        REG_WRITE(UART_RX_FILT_REG(0), (4 << UART_GLITCH_FILT_S) | UART_GLITCH_FILT_EN);
        REG_WRITE(UART_LOWPULSE_REG(0), 4095);
        REG_WRITE(UART_HIGHPULSE_REG(0), 4095);
        REG_SET_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN);
        return 0;
    }
    if (REG_READ(UART_RXD_CNT_REG(0)) < 300)
        return 0;
    const uint32_t low_period  = REG_READ(UART_LOWPULSE_REG(0));
    const uint32_t high_period = REG_READ(UART_HIGHPULSE_REG(0));
    REG_CLR_BIT(UART_CONF0_REG(0), UART_AUTOBAUD_EN);
    REG_CLR_BIT(UART_RX_FILT_REG(0), UART_GLITCH_FILT_EN);
    const int32_t measured = (int32_t)(UART_CLK_FREQ / (low_period + high_period + 2));
#elif defined(PLATFORM_ESP32)
    if (REG_GET_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN) == 0)
    {
        REG_WRITE(UART_AUTOBAUD_REG(0), 4 << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN);
        return 0;
    }
    if (REG_READ(UART_RXD_CNT_REG(0)) < 300)
        return 0;
    const int32_t low_period  = (int32_t)REG_READ(UART_LOWPULSE_REG(0));
    const int32_t high_period = (int32_t)REG_READ(UART_HIGHPULSE_REG(0));
    REG_CLR_BIT(UART_AUTOBAUD_REG(0), UART_AUTOBAUD_EN);
    const int32_t measured = 80000000 / (std::max(low_period, high_period) + 3);
#else
    const int32_t measured = (int32_t)currentBaud; // no HW autobaud (ESP8266/native), keep current baud
#endif
    DBGLN("MAVLink handset autobaud measured ~%d baud", (int)measured);
    uint32_t best = MAVLINK_HANDSET_BAUDS[0];
    for (uint32_t cand : MAVLINK_HANDSET_BAUDS)
    {
        if (abs(measured - (int32_t)best) > abs(measured - (int32_t)cand))
            best = cand;
    }
    return best;
}

int MAVLinkHandset::getMinPacketInterval() const
{
    if (currentBaud <= 115200) return 5000;
    if (currentBaud <= 400000) return 2000;
    return 1;
}

void MAVLinkHandset::setPacketInterval(int32_t PacketInterval)
{
    RequestedRCpacketInterval = PacketInterval;
    advertisedPeriodUs = (uint32_t)PacketInterval;
    computeChunkBudget(edgetxRcPeriodUs());
}

void MAVLinkHandset::processByte(uint8_t b)
{
    mavlink_message_t msg;
    mavlink_status_t status;

    if (!mavlink_parse_char(MAVLINK_COMM_0, b, &msg, &status))
        return;

    // ELRS config
    if (msg.msgid == MAVLINK_MSG_ID_TUNNEL)
    {
        mavlink_tunnel_t tun;
        mavlink_msg_tunnel_decode(&msg, &tun);
        if (tun.payload_type != MAV_TUNNEL_PAYLOAD_TYPE_ELRS_CRSF)
            return;

        const uint8_t *f = tun.payload;
        const uint32_t totalLen = (uint32_t)f[1] + 2;
        if (totalLen < 4 || totalLen > CRSF_MAX_PACKET_LEN || totalLen > tun.payload_length)
            return;
        if (crsfRouter.crsf_crc.calc(&f[2], totalLen - 3) != f[totalLen - 1])
        {
            DBGLN("config tunnel: CRSF CRC fail");
            return;
        }

        crsfRouter.processMessage(this, (const crsf_header_t *)f);
        return;
    }

    if (msg.msgid != MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE)
        return;

    mavlink_rc_channels_override_t rc;
    mavlink_msg_rc_channels_override_decode(&msg, &rc);

    // channels 17/18 are intentionally dropped
    const uint16_t raw[CRSF_NUM_CHANNELS] = {
        rc.chan1_raw,  rc.chan2_raw,  rc.chan3_raw,  rc.chan4_raw,
        rc.chan5_raw,  rc.chan6_raw,  rc.chan7_raw,  rc.chan8_raw,
        rc.chan9_raw,  rc.chan10_raw, rc.chan11_raw, rc.chan12_raw,
        rc.chan13_raw, rc.chan14_raw, rc.chan15_raw, rc.chan16_raw,
    };

    uint32_t channels[CRSF_NUM_CHANNELS];
    for (unsigned i = 0; i < CRSF_NUM_CHANNELS; i++)
    {
        if (raw[i] == 0 || raw[i] >= UINT16_MAX - 1)
        {
            channels[i] = CRSF_CHANNEL_VALUE_MID;
        }
        else
        {
            channels[i] = US_to_CRSF(constrain(raw[i], US_CHANNEL_VALUE_STD_MIN, US_CHANNEL_VALUE_STD_MAX));
        }
    }

    lastReceiveMs = millis();
    const uint32_t nowUs = micros();

    if (controllerConnected && lastRcFrameMicros != 0)
        rcFrameIntervalUs = nowUs - lastRcFrameMicros;
    lastRcFrameMicros = nowUs;
    frameJustReceived = true;

    if (!controllerConnected)
    {
        controllerConnected = true;
        baudConfirmed = true;
        rcFrameIntervalUs = 0;
#if defined(PLATFORM_ESP32)
        DBGLN("MAVLink handset connected (RX %s)", rxInverted ? "inverted" : "non-inverted");
#else
        DBGLN("MAVLink handset connected");
#endif
        if (connected) connected();
    }

    PerformChannelOverrides(channels, CRSF_NUM_CHANNELS);
    RCDataReceived(channels, CRSF_NUM_CHANNELS);
}

// Synthetic HEARTBEAT, enqueued only when no real FC heartbeat is flowing
uint8_t MAVLinkHandset::buildHeartbeat(uint8_t *buf)
{
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_GENERIC, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
    return (uint8_t)mavlink_msg_to_send_buffer(buf, &msg);
}

uint8_t MAVLinkHandset::buildRadioStatus(uint8_t *buf)
{
    mavlink_message_t msg;

    const uint8_t localLq  = linkStats.downlink_Link_quality * 254 / 100;
    const uint8_t remoteLq = linkStats.uplink_Link_quality * 254 / 100;
    mavlink_msg_radio_status_pack(1, 1, &msg, localLq, remoteLq, 100, 0, 0, 0, 0);
    return (uint8_t)mavlink_msg_to_send_buffer(buf, &msg);
}

void ICACHE_RAM_ATTR MAVLinkHandset::JustSentRFpacket()
{
}

// DEBUG_VECT name "ETXSYNC", x=refresh_rate_us, y=offset_us (plain us, always 0); EdgeTX decodes this
uint8_t MAVLinkHandset::buildSync(uint8_t *buf)
{
    const uint32_t now = millis();

    if (edgetxGapOpen() && (now - syncLastSentMs < SYNC_PACKET_INTERVAL_MS))
        return 0;

    const uint16_t rateUs = (uint16_t)std::min(advertisedPeriodUs, (uint32_t)65535);

    mavlink_message_t msg;
    mavlink_msg_debug_vect_pack(1, 1, &msg, "ETXSYNC", 0, (float)rateUs, 0.0f, 0.0f);
    syncLastSentMs = now;
    return (uint8_t)mavlink_msg_to_send_buffer(buf, &msg);
}

bool MAVLinkHandset::edgetxGapOpen() const
{
    if (!halfDuplex)
        return true;
    return advertisedPeriodUs != 0 && rcFrameIntervalUs != 0 && rcFrameIntervalUs >= advertisedPeriodUs / 2;
}

bool MAVLinkHandset::loadDueKeepalive(uint32_t now)
{
    if (uint8_t syncLen = buildSync(outBuf))
    {
        curRemaining = syncLen;
        curOffset = 0;
        return true;
    }

    if (!edgetxGapOpen())
        return false;

    if (loadNextConfigFrame())
        return true;

    if (connectionState == connectionState_e::connected)
    {
        if ((uint32_t)(now - lastRadioStatusMs) >= RADIO_STATUS_INTERVAL_MS)
        {
            lastRadioStatusMs = now;
            curRemaining = buildRadioStatus(outBuf);
            curOffset = 0;
            return true;
        }

        const bool fcHeartbeatLive = (lastFcHeartbeatMs != 0) && ((uint32_t)(now - lastFcHeartbeatMs) < FC_HEARTBEAT_PRESENT_MS);
        if (!fcHeartbeatLive && (uint32_t)(now - lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS)
        {
            lastHeartbeatMs = now;
            curRemaining = buildHeartbeat(outBuf);
            curOffset = 0;
            return true;
        }
    }

    return false;
}

bool MAVLinkHandset::popFrame(FIFO<MAVLINK_HANDSET_FIFO_SIZE> &fifo)
{
    fifo.lock();
    if (fifo.size() < 2)
    {
        fifo.unlock();
        return false;
    }

    const uint16_t frameLen = fifo.popSize();
    if (frameLen == 0 || frameLen > sizeof(outBuf) || fifo.size() < frameLen)
    {
        fifo.flush();
        fifo.unlock();
        return false;
    }

    fifo.popBytes(outBuf, frameLen);
    fifo.unlock();
    curRemaining = frameLen;
    curOffset = 0;
    return true;
}

bool MAVLinkHandset::loadNextFcFrame()
{
    if (!popFrame(downlinkFifo))
        return false;

    const uint32_t msgid = (outBuf[0] == MAVLINK_STX_V2)
        ? (outBuf[7] | ((uint32_t)outBuf[8] << 8) | ((uint32_t)outBuf[9] << 16))
        : outBuf[5];
    if (msgid == MAVLINK_MSG_ID_HEARTBEAT)
        lastFcHeartbeatMs = millis();
    return true;
}

static void pushFrame(FIFO<MAVLINK_HANDSET_FIFO_SIZE> &fifo, const uint8_t *frame, uint16_t len)
{
    fifo.lock();
    while (fifo.free() < (uint16_t)(len + 2) && fifo.size() >= 2)
    {
        const uint16_t oldLen = fifo.popSize();
        if (oldLen == 0 || oldLen > fifo.size())
        {
            fifo.flush();
            break;
        }
        fifo.skip(oldLen);
    }
    if (fifo.free() >= (uint16_t)(len + 2))
    {
        fifo.pushSize(len);
        fifo.pushBytes(frame, len);
    }
    fifo.unlock();
}

void MAVLinkHandset::forwardMessage(const crsf_header_t *message)
{
    if (!controllerConnected)
        return;

    const uint8_t size = CRSF_FRAME_SIZE(message->frame_size);
    if (size > CRSF_MAX_PACKET_LEN)
    {
        ERRLN("config frame too large");
        return;
    }

    uint8_t payload[MAVLINK_MSG_TUNNEL_FIELD_PAYLOAD_LEN] = {0};
    memcpy(payload, message, size);
    payload[0] = CRSF_SYNC_BYTE;

    mavlink_message_t msg;
    mavlink_msg_tunnel_pack(1, 1, &msg, 0, 0, MAV_TUNNEL_PAYLOAD_TYPE_ELRS_CRSF, size, payload);
    uint8_t frame[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = (uint16_t)mavlink_msg_to_send_buffer(frame, &msg);

    pushFrame(configFifo, frame, len);
}

bool MAVLinkHandset::loadNextConfigFrame()
{
    return popFrame(configFifo);
}

void MAVLinkHandset::handleOutput(uint32_t now)
{
    uint16_t budget = maxPacketBytes;

    while (budget > 0)
    {
        if (curRemaining == 0)
        {
            if (!loadDueKeepalive(now) && !(edgetxGapOpen() && loadNextFcFrame()))
                break;
        }

#if defined(PLATFORM_ESP32)
        if (halfDuplex && !transmitting)
        {
            transmitting = true;
            duplexSetTX();
        }
#endif

        uint16_t n = std::min(budget, curRemaining);
        CRSFHandset::Port.write(outBuf + curOffset, n);
        curOffset += n;
        curRemaining -= n;
        budget -= n;
    }
}

void MAVLinkHandset::queueTelemetry(const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
        frameDownlinkByte(data[i]);
}

void MAVLinkHandset::frameDownlinkByte(uint8_t b)
{
    if (inFrameIdx == 0)
    {
        if (b != MAVLINK_STX_V2 && b != MAVLINK_STX_V1)
            return;
        inFrameHdrLen = (b == MAVLINK_STX_V2) ? MAVLINK_V2_HEADER_LEN : MAVLINK_V1_HEADER_LEN;
        inFrameLen = 0;
    }

    inFrameBuf[inFrameIdx++] = b;

    if (inFrameLen == 0 && inFrameIdx >= inFrameHdrLen)
    {
        const uint8_t payloadLen = inFrameBuf[1];
        inFrameLen = (uint16_t)inFrameHdrLen + payloadLen + MAVLINK_CHECKSUM_LEN;
        if (inFrameBuf[0] == MAVLINK_STX_V2 && (inFrameBuf[2] & MAVLINK_IFLAG_SIGNED))
            inFrameLen += MAVLINK_V2_SIGNATURE_LEN;
    }

    if (inFrameLen != 0 && inFrameIdx >= inFrameLen)
    {
        emitDownlinkFrame(inFrameBuf, inFrameLen);
        inFrameIdx = 0;
        inFrameLen = 0;
    }
    else if (inFrameIdx >= sizeof(inFrameBuf))
    {
        inFrameIdx = 0;
        inFrameLen = 0;
    }
}

void MAVLinkHandset::emitDownlinkFrame(const uint8_t *frame, uint16_t len)
{
    pushFrame(downlinkFifo, frame, len);
}

void MAVLinkHandset::handleInput()
{
#if defined(PLATFORM_ESP32)
    if (halfDuplex && transmitting)
    {
        if (!uart_ll_is_tx_idle(UART_LL_GET_HW(0)))
        {
            return;
        }
        transmitting = false;
        setRxPolarity(rxInverted);

        while (CRSFHandset::Port.available())
        {
            CRSFHandset::Port.read();
        }
    }

    if (viaAutoDetect && detectionTimedOut())
    {
        restartDetection();
        return;
    }

    if (baudConfirmed && (millis() - lastReceiveMs) > BAUD_REACQUIRE_AFTER_MS)
    {
        baudConfirmed = false;
        baudMeasured = false;
    }

    if (!controllerConnected && !baudConfirmed)
    {
        if (baudMeasured && (millis() - baudMeasuredMs) > BAUD_REMEASURE_AFTER_MS)
            baudMeasured = false;

        if (!baudMeasured)
        {
            const uint32_t measured = hwAutobaud();
            if (measured != 0)
            {
                setBaud(measured);
                baudMeasured = true;
                baudMeasuredMs = millis();
                rxInverted = false;
                setRxPolarity(rxInverted);
                lastPolarityProbeMs = millis();
                while (CRSFHandset::Port.available()) CRSFHandset::Port.read();
            }
        }
        else if ((millis() - lastPolarityProbeMs) > POLARITY_PROBE_INTERVAL_MS)
        {
            lastPolarityProbeMs = millis();
            rxInverted = !rxInverted;
            setRxPolarity(rxInverted);
            while (CRSFHandset::Port.available()) CRSFHandset::Port.read();
        }
    }
#endif

    frameJustReceived = false;
    while (CRSFHandset::Port.available())
    {
        processByte((uint8_t)CRSFHandset::Port.read());
    }

    if (frameJustReceived && controllerConnected && !transmitting)
    {
        handleOutput(millis());
    }

    if (controllerConnected && (millis() - lastReceiveMs) > MAVLINK_HANDSET_TIMEOUT_MS)
    {
        controllerConnected = false;

        downlinkFifo.lock();
        downlinkFifo.flush();
        downlinkFifo.unlock();

        configFifo.lock();
        configFifo.flush();
        configFifo.unlock();
        curRemaining = 0;
        curOffset = 0;
        DBGLN("MAVLink handset disconnected");
        if (disconnected) disconnected();
#if defined(PLATFORM_ESP32)
        lastPolarityProbeMs = millis();
#endif
    }
}

#endif // TARGET_TX
