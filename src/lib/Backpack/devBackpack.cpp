#include "targets.h"

#include "common.h"
#include "device.h"
#include "msp.h"
#include "msptypes.h"
#include "CRSFHandset.h"
#include "config.h"
#include "logging.h"
#include "MAVLink.h"

#define BACKPACK_TIMEOUT 20    // How often to check for backpack commands

extern char backpackVersion[];
extern bool headTrackingEnabled;

bool TxBackpackWiFiReadyToSend = false;
bool VRxBackpackWiFiReadyToSend = false;
bool HTEnableFlagReadyToSend = false;
bool BackpackTelemReadyToSend = false;

bool lastRecordingState = false;

#if defined(PLATFORM_ESP32)

#define GPIO_PIN_BOOT0 0

#include "CRSF.h"
#include "hwTimer.h"

[[noreturn]] void startPassthrough()
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
        if (GPIO_PIN_DEBUG_RX == 44 && GPIO_PIN_DEBUG_TX == 43)
        {
            uplink = &Serial;
            Serial.setTxBufferSize(1024);
            Serial.setRxBufferSize(16384);
        }
        else
        {
            CRSFHandset::Port.begin(baud, SERIAL_8N1, 44, 43);  // pins are configured as 44 and 43
            CRSFHandset::Port.setTxBufferSize(1024);
            CRSFHandset::Port.setRxBufferSize(16384);
        }
        #else
        CRSFHandset::Port.begin(baud, SERIAL_8N1, 3, 1);  // default pin configuration 3 and 1
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

    const auto backpack = (HardwareSerial*)TxBackpack;
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
        int available_bytes = uplink->available();
        if (available_bytes > sizeof(buf))
            available_bytes = sizeof(buf);
        auto bytes_read = uplink->readBytes(buf, available_bytes);
        backpack->write(buf, bytes_read);

        available_bytes = backpack->available();
        if (available_bytes > sizeof(buf))
            available_bytes = sizeof(buf);
        bytes_read = backpack->readBytes(buf, available_bytes);
        uplink->write(buf, bytes_read);
    }
}

static int debouncedRead(int pin) {
    static const uint8_t min_matches = 100;

    static int last_state = -1;
    static uint8_t matches = 0;

    int current_state;

    current_state = digitalRead(pin);
    if (current_state == last_state) {
        matches = min(min_matches, (uint8_t)(matches + 1));
    } else {
        // We are bouncing. Reset the match counter.
        matches = 0;
        DBGLN("Bouncing!, current state: %d, last_state: %d, matches: %d", current_state, last_state, matches);
    }

    if (matches == min_matches) {
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
        if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
        {
            if (debouncedRead(GPIO_PIN_BOOT0) == 0)
            {
                startPassthrough();
            }
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
                if (resync_pos == sizeof(resync)) startPassthrough();
            }
            else
            {
                resync_pos = 0;
            }
        }
#endif
    }
}

static void BackpackWiFiToMSPOut(uint16_t command)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = command;
    packet.addByte(0);

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

static void BackpackHTFlagToMSPOut(uint8_t arg)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_HEAD_TRACKING;
    packet.addByte(arg);

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

void BackpackBinding()
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BIND;
    for (unsigned b=0; b<UID_LEN; ++b)
    {
        packet.addByte(UID[b]);
    }

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

uint8_t GetDvrDelaySeconds(uint8_t index)
{
    constexpr uint8_t delays[] = {0, 5, 15, 30, 45, 60, 120};
    return delays[index >= sizeof(delays) ? 0 : index];
}

static void AuxStateToMSPOut()
{
    if (config.GetDvrAux() == 0)
    {
        // DVR AUX control is off
        return;
    }

    const uint8_t auxNumber = (config.GetDvrAux() - 1) / 2 + 4;
    const uint8_t auxInverted = (config.GetDvrAux() + 1) % 2;

    const bool recordingState = CRSF_to_BIT(ChannelData[auxNumber]) ^ auxInverted;

    if (recordingState == lastRecordingState)
    {
        // Channel state has not changed since we last checked
        return;
    }
    lastRecordingState = recordingState;

    const uint16_t delay = GetDvrDelaySeconds(recordingState ? config.GetDvrStartDelay() : config.GetDvrStopDelay());

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_RECORDING_STATE;
    packet.addByte(recordingState);
    packet.addByte(delay & 0xFF); // delay byte 1
    packet.addByte(delay >> 8); // delay byte 2

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

void sendCRSFTelemetryToBackpack(uint8_t *data)
{
    if (config.GetBackpackTlmMode() == BACKPACK_TELEM_MODE_OFF)
    {
        // Backpack telem is off
        return;
    }

    if (config.GetLinkMode() == TX_MAVLINK_MODE)
    {
        // Tx is in MAVLink mode, don't forward CRSF telemetry
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

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

void sendMAVLinkTelemetryToBackpack(uint8_t *data)
{
    if (config.GetBackpackTlmMode() == BACKPACK_TELEM_MODE_OFF)
    {
        // Backpack telem is off
        return;
    }

    uint8_t count = data[1];
    TxBackpack->write(data + CRSF_FRAME_NOT_COUNTED_BYTES, count);
}

void sendConfigToBackpack()
{
    // Send any config values to the tx-backpack, as one key/value pair per MSP msg
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_CONFIG;
    packet.addByte(MSP_ELRS_BACKPACK_CONFIG_TLM_MODE); // Backpack tlm mode
    packet.addByte(config.GetBackpackTlmMode());
    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

static bool initialize()
{
    if (OPT_USE_TX_BACKPACK)
    {
        if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_BOOT0, INPUT); // setup so we can detect pinchange for passthrough mode
            pinMode(GPIO_PIN_BACKPACK_BOOT, OUTPUT);
            pinMode(GPIO_PIN_BACKPACK_EN, OUTPUT);
            // Shut down the backpack via EN pin and hold it there until the first event()
            digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);   // enable low
            digitalWrite(GPIO_PIN_BACKPACK_BOOT, LOW); // bootloader pin high
            delay(20);
            // Rely on event() to boot
        }
        handset->setRCDataCallback(AuxStateToMSPOut);
    }
    return OPT_USE_TX_BACKPACK;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    static uint8_t versionRequestTries = 0;
    static uint32_t lastVersionTryTime = 0;

    if (InBindingMode)
    {
        BackpackBinding();
        return 1000;        // don't check for another second so we don't spam too hard :-)
    }

    if (versionRequestTries < 10 && strlen(backpackVersion) == 0 && (lastVersionTryTime == 0 || millis() - lastVersionTryTime > 1000)) {
        lastVersionTryTime = millis();
        versionRequestTries++;
        mspPacket_t out;
        out.reset();
        out.makeCommand();
        out.function = MSP_ELRS_GET_BACKPACK_VERSION;
        MSP::sendPacket(&out, TxBackpack);
        DBGLN("Sending get backpack version command");
    }

    if (TxBackpackWiFiReadyToSend && connectionState < MODE_STATES)
    {
        TxBackpackWiFiReadyToSend = false;
        BackpackWiFiToMSPOut(MSP_ELRS_SET_TX_BACKPACK_WIFI_MODE);
    }

    if (VRxBackpackWiFiReadyToSend && connectionState < MODE_STATES)
    {
        VRxBackpackWiFiReadyToSend = false;
        BackpackWiFiToMSPOut(MSP_ELRS_SET_VRX_BACKPACK_WIFI_MODE);
    }

    if (HTEnableFlagReadyToSend && connectionState < MODE_STATES)
    {
        HTEnableFlagReadyToSend = false;
        BackpackHTFlagToMSPOut(headTrackingEnabled);
    }

    if (BackpackTelemReadyToSend && connectionState < MODE_STATES)
    {
        BackpackTelemReadyToSend = false;
        sendConfigToBackpack();
    }

    return BACKPACK_TIMEOUT;
}

static int event()
{
    if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
    {
        // EN should be HIGH to be active
        digitalWrite(GPIO_PIN_BACKPACK_EN, (config.GetBackpackDisable() || connectionState == bleJoystick || connectionState == wifiUpdate) ? LOW : HIGH);
    }

    return DURATION_IGNORE;
}

device_t Backpack_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED | EVENT_CONFIG_MAIN_CHANGED
};
#endif
