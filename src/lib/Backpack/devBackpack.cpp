#include "targets.h"

#include "CRSFHandset.h"
#include "CRSFRouter.h"
#include "config.h"
#include "device.h"
#include "logging.h"
#include "msp.h"
#include "msptypes.h"
#include "telemetry.h"

// How often to check for backpack commands
#define BACKPACK_PERIOD_MS  20
// value in ptrChannelData that means "don't replace value in ChannelData"
#define HT_DO_NOT_UPDATE    0xffff
// EdgeTX doesn't support not updating a value, so 0 might be a safer value?
// it always takes (11-bits) - CRSF_CHANNEL_VALUE_MID * 5/8
#define HT_EDGETX_NO_VALUE  0
// Stop overriding channels with PTR data if older than this
#define HT_STALE_TIMEOUT_MS 1000

extern char backpackVersion[];

bool TxBackpackWiFiReadyToSend = false;
bool VRxBackpackWiFiReadyToSend = false;
bool BackpackTelemReadyToSend = false;
bool lastRecordingState = false;

static uint16_t ptrChannelData[CRSF_NUM_CHANNELS];
static bool headTrackingEnabled = false;
static uint32_t lastPTRValidTimeMs;

#if defined(PLATFORM_ESP32)

#define GPIO_PIN_BOOT0 0

#include "hwTimer.h"

[[noreturn]] static void startPassthrough(const bool useUSB = false)
{
    // stop everything
    devicesStop();
    Radio.End();
    hwTimer::stop();
    handset->End();

    Stream *uplink = &CRSFHandset::Port;

    uint32_t baud = PASSTHROUGH_BAUD == -1 ? BACKPACK_LOGGING_BAUD : PASSTHROUGH_BAUD;
    // get ready for passthrough
    if (GPIO_PIN_RCSIGNAL_RX == GPIO_PIN_RCSIGNAL_TX)
    {
#if defined(PLATFORM_ESP32_S3)
        // if UART0 is connected to the backpack then use the USB for the uplink
        if (useUSB)
        {
            uplink = &Serial;
            Serial.setTxBufferSize(1024);
            Serial.setRxBufferSize(16384);
        }
        else
        {
            CRSFHandset::Port.begin(baud, SERIAL_8N1, 44, 43); // pins are configured as 44 and 43
            CRSFHandset::Port.setTxBufferSize(1024);
            CRSFHandset::Port.setRxBufferSize(16384);
        }
#else
        CRSFHandset::Port.begin(baud, SERIAL_8N1, 3, 1); // default pin configuration 3 and 1
        CRSFHandset::Port.setTxBufferSize(1024);
        CRSFHandset::Port.setRxBufferSize(16384);
#endif
    }
    else
    {
        CRSFHandset::Port.begin(baud, SERIAL_8N1, GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
        CRSFHandset::Port.setTxBufferSize(1024);
        CRSFHandset::Port.setRxBufferSize(16384);
    }
    disableLoopWDT();

    const auto backpack = (HardwareSerial *)BackpackOrLogStrm;
    if (baud != BACKPACK_LOGGING_BAUD)
    {
        backpack->begin(PASSTHROUGH_BAUD, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
    }
    backpack->setRxBufferSize(1024);
    backpack->setTxBufferSize(16384);

    // reset ESP8285 into bootloader mode
    digitalWrite(GPIO_PIN_BACKPACK_BOOT, HIGH);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, HIGH);
    delay(50);

    uplink->flush();
    backpack->flush();

    uint8_t buf[64];
    while (backpack->available())
    {
        backpack->readBytes(buf, sizeof(buf));
    }

    // go hard!
    for (;;)
    {
        int available_bytes = min(uplink->available(), static_cast<int>(sizeof(buf)));
        auto bytes_read = uplink->readBytes(buf, available_bytes);
        backpack->write(buf, bytes_read);

        available_bytes = min(backpack->available(), static_cast<int>(sizeof(buf)));
        bytes_read = backpack->readBytes(buf, available_bytes);
        uplink->write(buf, bytes_read);
    }
}

static int debouncedRead(int pin)
{
    static constexpr uint8_t min_matches = 100;

    static int last_state = -1;
    static uint8_t matches = 0;

    const int current_state = digitalRead(pin);
    if (current_state == last_state)
    {
        matches = min(min_matches, static_cast<uint8_t>(matches + 1));
    }
    else
    {
        // We are bouncing. Reset the match counter.
        matches = 0;
        DBGLN("Bouncing!, current state: %d, last_state: %d, matches: %d", current_state, last_state, matches);
    }

    if (matches == min_matches)
    {
        // We have a stable state and report it.
        return current_state;
    }

    last_state = current_state;

    // We don't have a definitive state we could report.
    return -1;
}

void checkBackpackUpdate()
{
    if (OPT_USE_TX_BACKPACK)
    {
        if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN && debouncedRead(GPIO_PIN_BOOT0) == 0)
        {
            startPassthrough();
        }
#if defined(PLATFORM_ESP32_S3)
        // Start passthrough mode if an Espressif resync packet is detected on the USB port
        static const uint8_t resync[] = {
            0xc0,0x00,0x08,0x24,0x00,0x00,0x00,0x00,0x00,0x07,0x07,0x12,0x20,0x55,0x55,0x55,0x55,
            0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55, 0x55,0x55,
            0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0xc0
        };
        static int resync_pos = 0;
        while(Serial.available())
        {
            int byte = Serial.read();
            if (byte == resync[resync_pos])
            {
                resync_pos++;
                if (resync_pos == sizeof(resync)) startPassthrough(true);
            }
            else
            {
                resync_pos = 0;
            }
        }
#endif
    }
}

static void BackpackWiFiToMSPOut(const uint16_t command)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = command;
    packet.addByte(0);

    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

static void BackpackHTFlagToMSPOut(const uint8_t arg)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_HEAD_TRACKING;
    packet.addByte(arg);

    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

static uint8_t GetDvrDelaySeconds(const uint8_t index)
{
    constexpr uint8_t delays[] = {0, 5, 15, 30, 45, 60, 120};
    return delays[index >= sizeof(delays) ? 0 : index];
}

static void BackpackDvrRecordingStateMSPOut(bool recordingState)
{
    uint16_t delay = 0;

    if (recordingState)
    {
        delay = GetDvrDelaySeconds(config.GetDvrStartDelay());
    }
    else
    {
        delay = GetDvrDelaySeconds(config.GetDvrStopDelay());
    }

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_RECORDING_STATE;
    packet.addByte(recordingState);
    packet.addByte(delay & 0xFF); // delay byte 1
    packet.addByte(delay >> 8);   // delay byte 2

    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

static void BackpackBinding()
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BIND;
    for (const uint8_t b : UID)
    {
        packet.addByte(b);
    }

    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

/***
 * @brief:  Read 16-bit CRSF value channels from the packet payload and store them for head-tracking/trainer.
 *          Process as many values as there are payload bytes available
 *          A value of HT_DO_NOT_UPDATE will not overwrite that channel
 */
void processPanTiltRollPacket(const uint32_t now, const mspPacket_t *packet)
{
    unsigned chStart = (config.GetPTRStartChannel() == HT_START_EDGETX) ? 0 : (config.GetPTRStartChannel() - HT_START_AUX1 + AUX1);
    unsigned payloadPos = 0;
    // Any time a PTR packet is received, all channels are returned to HT_DO_NOT_UPDATE unless they are in this packet
    for (unsigned ch=0; ch<CRSF_NUM_CHANNELS; ++ch)
    {
        uint16_t chVal;
        if (ch >= chStart && payloadPos < (packet->payloadSize - 1))
        {
            chVal = packet->payload[payloadPos] | (packet->payload[payloadPos+1] << 8);
            payloadPos += 2;
        }
        else
        {
            chVal = HT_DO_NOT_UPDATE;
        }
        ptrChannelData[ch] = chVal;
    }

    lastPTRValidTimeMs = now;
}

static void headtrackPublishChannelsToEdgeTX()
{
    static uint32_t lastPTRSentMs = 0;
    if (lastPTRSentMs == lastPTRValidTimeMs)
        return;
    lastPTRSentMs = lastPTRValidTimeMs;

    CRSF_MK_FRAME_T(crsf_channels_t) rcPacket; // not zeroed, entire packet will be filled
    // Pack the ptrChannelData into 11 bits for the crsf_channels_t packet
    constexpr unsigned dstBits = 11;
    uint8_t *dst = reinterpret_cast<uint8_t *>(&rcPacket.p);
    uint32_t accumulator = 0;
    uint32_t bitCnt = 0;
    for (unsigned ch=0; ch<CRSF_NUM_CHANNELS; ++ch)
    {
        uint32_t val = ptrChannelData[ch] == HT_DO_NOT_UPDATE ? HT_EDGETX_NO_VALUE : ptrChannelData[ch];
        accumulator |= val << bitCnt;
        bitCnt += dstBits;
        while (bitCnt >= 8)
        {

            *dst++ = accumulator;
            accumulator >>= 8;
            bitCnt -= 8;
        }
    }
    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&rcPacket, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, sizeof(rcPacket) - 2, CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.deliverMessage(nullptr, &rcPacket.h);
}

void headtrackOverrideChannels(uint32_t channels[], size_t channelCount)
{
    if (millis() - lastPTRValidTimeMs > HT_STALE_TIMEOUT_MS)
        return;

    channelCount = std::min((size_t)CRSF_NUM_CHANNELS, channelCount);
    for (unsigned ch=0; ch<channelCount; ++ch)
    {
        const auto val = ptrChannelData[ch];
        if (val != HT_DO_NOT_UPDATE)
            channels[ch] = val;
    }
}

static void BackpackPollAuxStates()
{
    // HeadTracking Enable
    bool enable = backpackVersion[0] != 0;
    if (enable)
    {
        switch (config.GetPTREnableChannel())
        {
            case HT_OFF:
                enable = false; break;
            case HT_ON:
                enable = true; break;
            default:
                enable = CRSF_to_BIT(ChannelData[config.GetPTREnableChannel() / 2 + 3]);
                if (config.GetPTREnableChannel() % 2)
                    enable = !enable;
        }
    }
    if (enable != headTrackingEnabled)
    {
        headTrackingEnabled = enable;
        BackpackHTFlagToMSPOut(headTrackingEnabled);

        auto rcchannelsoverride_cb = (enable && config.GetPTRStartChannel() != HT_START_EDGETX) ? &headtrackOverrideChannels : nullptr;
        handset->setRcChannelsOverrideCallback(rcchannelsoverride_cb);

        auto rcdata_cb = (enable && config.GetPTRStartChannel() == HT_START_EDGETX) ? &headtrackPublishChannelsToEdgeTX : nullptr;
        handset->setRCDataCallback(rcdata_cb);
    }

    // DVR recording enable
    if (config.GetDvrAux() != 0)
    {
        // DVR AUX control is on
        const uint8_t auxNumber = (config.GetDvrAux() - 1) / 2 + AUX1;
        const uint8_t auxInverted = (config.GetDvrAux() + 1) % 2;

        const bool recordingState = CRSF_to_BIT(ChannelData[auxNumber]) ^ auxInverted;
        if (recordingState != lastRecordingState)
        {
            // Channel state has changed since we last checked, so schedule a MSP send
            lastRecordingState = recordingState;
            BackpackDvrRecordingStateMSPOut(recordingState);
        }
    }
}

void sendCRSFTelemetryToBackpack(const uint8_t *data)
{
    if (config.GetBackpackDisable() || config.GetBackpackTlmMode() == BACKPACK_TELEM_MODE_OFF || config.GetLinkMode() == TX_MAVLINK_MODE)
    {
        return;
    }

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_CRSF_TLM;

    uint8_t size = CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX]);
    if (size > CRSF_MAX_PACKET_LEN)
    {
        ERRLN("CRSF frame exceeds max length");
        return;
    }

    for (uint8_t i = 0; i < size; ++i)
    {
        packet.addByte(data[i]);
    }

    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

void sendMAVLinkTelemetryToBackpack(const uint8_t *data)
{
    if (config.GetBackpackDisable() || config.GetBackpackTlmMode() == BACKPACK_TELEM_MODE_OFF)
    {
        // Backpack telemetry is off
        return;
    }

    const uint8_t count = data[1];
    BackpackOrLogStrm->write(data + CRSF_FRAME_NOT_COUNTED_BYTES, count);
}

static void sendConfigToBackpack()
{
    // Send any config values to the tx-backpack, as one key/value pair per MSP msg
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_CONFIG;
    packet.addByte(MSP_ELRS_BACKPACK_CONFIG_TLM_MODE); // Backpack tlm mode
    packet.addByte(config.GetBackpackTlmMode());
    MSP::sendPacket(&packet, BackpackOrLogStrm); // send to tx-backpack as MSP
}

static bool initialize()
{
    if (OPT_USE_TX_BACKPACK)
    {
        if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_BOOT0, INPUT); // setup so we can detect pin-change for passthrough mode
            pinMode(GPIO_PIN_BACKPACK_BOOT, OUTPUT);
            pinMode(GPIO_PIN_BACKPACK_EN, OUTPUT);
            // Shut down the backpack via EN pin and hold it there until the first event()
            digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);   // enable low
            digitalWrite(GPIO_PIN_BACKPACK_BOOT, LOW); // bootloader pin high
            delay(20);
            // Rely on event() to boot
        }
        // Set all channels of PTR data to "do not override" (0xffff)
        memset(ptrChannelData, 0xff, sizeof(ptrChannelData));
    }
    return OPT_USE_TX_BACKPACK;
}

static int start()
{
    return config.GetBackpackDisable() ? DURATION_NEVER : DURATION_IMMEDIATELY;
}

static int timeout()
{
    static uint8_t versionRequestTries = 0;
    static uint32_t lastVersionTryTime = 0;

    if (versionRequestTries < 10 && strlen(backpackVersion) == 0 && (lastVersionTryTime == 0 || millis() - lastVersionTryTime > 1000))
    {
        lastVersionTryTime = millis();
        versionRequestTries++;
        mspPacket_t out;
        out.reset();
        out.makeCommand();
        out.function = MSP_ELRS_GET_BACKPACK_VERSION;
        MSP::sendPacket(&out, BackpackOrLogStrm);
        DBGLN("Sending get backpack version command");
    }

    if (connectionState < MODE_STATES && !config.GetBackpackDisable())
    {
        if (TxBackpackWiFiReadyToSend)
        {
            TxBackpackWiFiReadyToSend = false;
            BackpackWiFiToMSPOut(MSP_ELRS_SET_TX_BACKPACK_WIFI_MODE);
        }

        if (VRxBackpackWiFiReadyToSend)
        {
            VRxBackpackWiFiReadyToSend = false;
            BackpackWiFiToMSPOut(MSP_ELRS_SET_VRX_BACKPACK_WIFI_MODE);
        }

        if (BackpackTelemReadyToSend)
        {
            BackpackTelemReadyToSend = false;
            sendConfigToBackpack();
        }

        BackpackPollAuxStates();
    }

    return BACKPACK_PERIOD_MS;
}

static int event()
{
    const bool disabled = config.GetBackpackDisable() || connectionState == bleJoystick || connectionState == wifiUpdate;
    if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
    {
        // EN should be HIGH to be active
        digitalWrite(GPIO_PIN_BACKPACK_EN, disabled ? LOW : HIGH);
    }

    if (InBindingMode)
    {
        BackpackBinding();
    }

    if (disabled && headTrackingEnabled)
    {
        // Disconnect any handlers that might be active. This is done blindly and will disconnect callbacks from other
        // devices if they are assigned in their own event() handlers that fire before this!
        // Note there's no MSP_ELRS_BACKPACK_SET_PTR sent either, as the backpack is disabled above
        headTrackingEnabled = false;
        handset->setRcChannelsOverrideCallback(nullptr);
        handset->setRCDataCallback(nullptr);
    }

    return disabled ? DURATION_NEVER : BACKPACK_PERIOD_MS;
}

device_t Backpack_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED | EVENT_CONFIG_MAIN_CHANGED | EVENT_ENTER_BIND_MODE
};
#endif
