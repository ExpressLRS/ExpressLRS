#include "targets.h"
#include "common.h"
#include "device.h"
#include "msp.h"
#include "msptypes.h"
#include "CRSF.h"
#include "config.h"
#include "logging.h"

#define BACKPACK_TIMEOUT 20    // How often to check for backpack commands

extern bool InBindingMode;
extern Stream *TxBackpack;
extern char backpackVersion[];

bool TxBackpackWiFiReadyToSend = false;
bool VRxBackpackWiFiReadyToSend = false;

bool lastRecordingState = false;

#if defined(GPIO_PIN_BACKPACK_EN)

#ifndef PASSTHROUGH_BAUD
#define PASSTHROUGH_BAUD BACKPACK_LOGGING_BAUD
#endif

#include "CRSF.h"
#include "hwTimer.h"

void startPassthrough()
{
    // stop everyhting
    devicesStop();
    Radio.End();
    hwTimer::stop();
    CRSF::End();

    uint32_t baud = PASSTHROUGH_BAUD == -1 ? BACKPACK_LOGGING_BAUD : PASSTHROUGH_BAUD;
    // get ready for passthrough
    if (GPIO_PIN_RCSIGNAL_RX == GPIO_PIN_RCSIGNAL_TX)
    {
        // if we have a single S.PORT pin for RX then we assume the standard UART pins for passthrough
        CRSF::Port.begin(baud, SERIAL_8N1, 3, 1);
    }
    else
    {
        CRSF::Port.begin(baud, SERIAL_8N1, GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
    }
    disableLoopWDT();

    HardwareSerial &backpack = *(HardwareSerial*)TxBackpack;
    if (baud != BACKPACK_LOGGING_BAUD)
    {
        backpack.begin(PASSTHROUGH_BAUD, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
    }

    // reset ESP8285 into bootloader mode
    digitalWrite(GPIO_PIN_BACKPACK_BOOT, HIGH);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, HIGH);
    delay(50);

    CRSF::Port.flush();
    backpack.flush();

    uint8_t buf[64];
    while (backpack.available())
        backpack.readBytes(buf, sizeof(buf));

    // go hard!
    for (;;)
    {
        int r = CRSF::Port.available();
        if (r > sizeof(buf))
            r = sizeof(buf);
        r = CRSF::Port.readBytes(buf, r);
        backpack.write(buf, r);

        r = backpack.available();
        if (r > sizeof(buf))
            r = sizeof(buf);
        r = backpack.readBytes(buf, r);
        CRSF::Port.write(buf, r);
    }
}
#endif

void checkBackpackUpdate()
{
#if defined(GPIO_PIN_BACKPACK_EN)
    if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
    {
        if (!digitalRead(0))
        {
            startPassthrough();
        }
    }
#endif
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

void BackpackBinding()
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BIND;
    packet.addByte(MasterUID[0]);
    packet.addByte(MasterUID[1]);
    packet.addByte(MasterUID[2]);
    packet.addByte(MasterUID[3]);
    packet.addByte(MasterUID[4]);
    packet.addByte(MasterUID[5]);

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
}

uint8_t GetDvrDelaySeconds(uint8_t index)
{
    constexpr uint8_t delays[] = {0, 5, 15, 30, 45, 60, 120};
    return delays[index >= sizeof(delays) ? 0 : index];
}

static void AuxStateToMSPOut()
{
#if defined(USE_TX_BACKPACK)
    if (config.GetDvrAux() == 0)
    {
        // DVR AUX control is off
        return;
    }

    uint8_t auxNumber = (config.GetDvrAux() - 1) / 2 + 4;
    uint8_t auxInverted = (config.GetDvrAux() + 1) % 2;

    bool recordingState = CRSF_to_BIT(ChannelData[auxNumber]) ^ auxInverted;

    if (recordingState == lastRecordingState)
    {
        // Channel state has not changed since we last checked
        return;
    }
    lastRecordingState = recordingState;

    uint16_t delay = 0;

    if (recordingState)
    {
        delay = GetDvrDelaySeconds(config.GetDvrStartDelay());
    }

    if (!recordingState)
    {
        delay = GetDvrDelaySeconds(config.GetDvrStopDelay());
    }

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_RECORDING_STATE;
    packet.addByte(recordingState);
    packet.addByte(delay & 0xFF); // delay byte 1
    packet.addByte(delay >> 8); // delay byte 2

    MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
#endif // USE_TX_BACKPACK
}

static void initialize()
{
#if defined(GPIO_PIN_BACKPACK_EN)
    if (GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
    {
        pinMode(0, INPUT); // setup so we can detect pinchange for passthrough mode
        pinMode(GPIO_PIN_BACKPACK_BOOT, OUTPUT);
        pinMode(GPIO_PIN_BACKPACK_EN, OUTPUT);
        // Shut down the backpack via EN pin and hold it there until the first event()
        digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);   // enable low
        digitalWrite(GPIO_PIN_BACKPACK_BOOT, LOW); // bootloader pin high
        delay(20);
        // Rely on event() to boot
    }
#endif

    CRSF::RCdataCallback = AuxStateToMSPOut;
}

static int start()
{
    if (OPT_USE_TX_BACKPACK)
    {
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
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
    return BACKPACK_TIMEOUT;
}

static int event()
{
#if defined(GPIO_PIN_BACKPACK_EN)
    if (OPT_USE_TX_BACKPACK && GPIO_PIN_BACKPACK_EN != UNDEF_PIN)
    {
        // EN should be HIGH to be active
        digitalWrite(GPIO_PIN_BACKPACK_EN, config.GetBackpackDisable() ? LOW : HIGH);
    }
#endif
    return DURATION_IGNORE;
}

device_t Backpack_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};
