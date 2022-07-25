#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#include "CRSF.h"
#include "FHSS.h"
#include "NimBLEDevice.h"
#include "POWERMGNT.h"
#include "common.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"

NimBLEServer *pServer;
NimBLECharacteristic *rcState;
NimBLECharacteristic *rcCRSF;
NimBLECharacteristic *rcLink;

NimBLEService *rcTemp;
NimBLECharacteristic *cTemp;

unsigned short const TELEMETRY_SVC_UUID = 0x1819;
unsigned short const TELEMETRY_STATE_UUID = 0x2B05;
unsigned short const TELEMETRY_CRSF_UUID = 0x2A4D;
unsigned short const TELEMETRY_LINK_UUID = 0x2BBD;

unsigned short const DEVICE_INFO_SVC_UUID = 0x180A;
unsigned short const MODEL_NUMBER_SVC_UUID = 0x2A24;
unsigned short const SERIAL_NUMBER_SVC_UUID = 0x2A25;
unsigned short const SOFTWARE_NUMBER_SVC_UUID = 0x2A28;
unsigned short const HARDWARE_NUMBER_SVC_UUID = 0x2A27;
unsigned short const MANUFACTURER_NAME_SVC_UUID = 0x2A29;

unsigned short const TEMP_SVC_UUID = 0x181A;
unsigned short const TEMP_C_UUID = 0x2A1C;

#ifdef HAS_THERMAL
#include "thermal.h"
extern Thermal thermal;
#endif

extern CRSF crsf;

String getMasterUIDString()
{
    char muids[7] = {0};
    sprintf(muids, "%02X%02X%02X", MasterUID[3], MasterUID[4], MasterUID[5]);
    return String(muids);
}

void ICACHE_RAM_ATTR BluetoothTelemetrykUpdateValues(uint8_t *data)
{
    rcState->setValue(connectionState);
    rcState->notify(true);

    if (data != nullptr)
    {
        rcCRSF->setValue(data);
        rcCRSF->notify();
    }

    cTemp->setValue(thermal.read_temp());
    cTemp->notify();

    uint8_t linkstats[sizeof(crsfPayloadLinkstatistics_s)];
    memcpy(linkstats, (byte *)&CRSF::LinkStatistics, sizeof(crsfPayloadLinkstatistics_s));
    rcLink->setValue(linkstats);
    rcLink->notify();
}

void BluetoothTelemetryBegin()
{

    // bleGamepad is null if it hasn't been started yet
    if (pServer != nullptr)
        return;

    NimBLEDevice::init(String(String(device_name) + " " + getMasterUIDString()).c_str());

    /** Set low transmit power, default is 6db */
    BLEDevice::setPower(ESP_PWR_LVL_P6);
    pServer = NimBLEDevice::createServer();
    NimBLEService *rcService = pServer->createService(TELEMETRY_SVC_UUID);
    rcState = rcService->createCharacteristic(TELEMETRY_STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    rcCRSF = rcService->createCharacteristic(TELEMETRY_CRSF_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, CRSF_MAX_PACKET_LEN);
    rcLink = rcService->createCharacteristic(TELEMETRY_LINK_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, LinkStatisticsFrameLength);
    rcService->start();

#ifdef HAS_THERMAL
    rcTemp = pServer->createService(TEMP_SVC_UUID);
    cTemp = rcTemp->createCharacteristic(TEMP_C_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    cTemp->setValue(String(thermal.getTempValue()));
    rcTemp->start();
#endif

    auto dInfo = pServer->createService(DEVICE_INFO_SVC_UUID);
    dInfo
        ->createCharacteristic(MANUFACTURER_NAME_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue("ExpressLRS");
    dInfo
        ->createCharacteristic(MODEL_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue(String(product_name));
    dInfo
        ->createCharacteristic(SERIAL_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue(String(__TIMESTAMP__) + " - " + getMasterUIDString());
    dInfo
        ->createCharacteristic(SOFTWARE_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue(String(version));
    dInfo
        ->createCharacteristic(HARDWARE_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue(String(getRegulatoryDomain()));

    dInfo->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(rcService->getUUID());

#ifdef HAS_THERMAL
    pAdvertising->addServiceUUID(rcTemp->getUUID());
#endif

    pAdvertising->addServiceUUID(dInfo->getUUID());
    pAdvertising->start();

    INFOLN("Starting BLE Telemetry!");
}

static int start()
{
    BluetoothTelemetryBegin();
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    BluetoothTelemetrykUpdateValues(nullptr);
    return 500;
}

device_t BLET_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout};

#endif