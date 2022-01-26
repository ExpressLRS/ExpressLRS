#include "targets.h"
#include "common.h"
#include "device.h"
#include "msp.h"
#include "msptypes.h"

#define BACKPACK_TIMEOUT 20    // How often to chech for backpack commands

extern bool InBindingMode;
extern Stream *LoggingBackpack;

bool TxBackpackWiFiReadyToSend = false;
bool VRxBackpackWiFiReadyToSend = false;

#if defined(GPIO_PIN_BACKPACK_EN) && GPIO_PIN_BACKPACK_EN != UNDEF_PIN

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
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

    // get ready for passthrough
    CRSF::Port.begin(460800, SERIAL_8N1, GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
    HardwareSerial &backpack = *((HardwareSerial*)LoggingBackpack);
    backpack.begin(460800, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
    disableLoopWDT();

    // reset ESP8285 into bootloader mode
    digitalWrite(GPIO_PIN_BACKPACK_BOOT, HIGH);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);
    delay(100);
    digitalWrite(GPIO_PIN_BACKPACK_EN, HIGH);
    delay(50);
    digitalWrite(GPIO_PIN_BACKPACK_BOOT, LOW);

    CRSF::Port.flush();
    backpack.flush();

    uint8_t buf[64];
    while (backpack.available())
        backpack.read(buf, sizeof(buf));

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

static void BackpackWiFiToMSPOut(uint16_t command)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = command;
    packet.addByte(0);

    MSP::sendPacket(&packet, LoggingBackpack); // send to tx-backpack as MSP
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

    MSP::sendPacket(&packet, LoggingBackpack); // send to tx-backpack as MSP
}

static void initialize()
{
#if defined(GPIO_PIN_BACKPACK_EN) && GPIO_PIN_BACKPACK_EN != UNDEF_PIN
    pinMode(0, INPUT); // setup so we can detect pinchange for passthrough mode
    // reset the ESP8285 so we know it's running
    pinMode(GPIO_PIN_BACKPACK_BOOT, OUTPUT);
    pinMode(GPIO_PIN_BACKPACK_EN, OUTPUT);
    digitalWrite(GPIO_PIN_BACKPACK_EN, LOW);   // enable low
    digitalWrite(GPIO_PIN_BACKPACK_BOOT, LOW); // bootloader pin high
    delay(50);
    digitalWrite(GPIO_PIN_BACKPACK_EN, HIGH); // enable high
#endif
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (InBindingMode)
    {
        BackpackBinding();
        return 1000;        // don't check for another second so we don't spam too hard :-)
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

#if defined(GPIO_PIN_BACKPACK_EN) && GPIO_PIN_BACKPACK_EN != UNDEF_PIN
    if (!digitalRead(0))
    {
        startPassthrough();
        return DURATION_NEVER;
    }
    return BACKPACK_TIMEOUT;
#else
    return DURATION_NEVER;
#endif
}

device_t Backpack_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};
