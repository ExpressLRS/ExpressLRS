#if defined(PLATFORM_ESP32)

#include "CRSF.h"
#include "FHSS.h"
#include "NimBLEDevice.h"
#include "POWERMGNT.h"
#include "common.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"

#include "devBLETelemetry.h"

#include "config.h"
extern TxConfig config;

NimBLEServer *pServer;
NimBLECharacteristic *rcCRSF;


//how often link stats packet is sent over BLE
#define BLE_LINKSTATS_PACKET_PERIOD_MS 200

//how often channels(sticks positions) packet is sent over BLE
#define BLE_CHANNELS_PACKET_PERIOD_MS 300

unsigned short const TELEMETRY_SVC_UUID = 0x1819;
unsigned short const TELEMETRY_CRSF_UUID = 0x2BBD;

unsigned short const DEVICE_INFO_SVC_UUID = 0x180A;
unsigned short const MODEL_NUMBER_SVC_UUID = 0x2A24;
unsigned short const SERIAL_NUMBER_SVC_UUID = 0x2A25;
unsigned short const SOFTWARE_NUMBER_SVC_UUID = 0x2A28;
unsigned short const HARDWARE_NUMBER_SVC_UUID = 0x2A27;
unsigned short const MANUFACTURER_NAME_SVC_UUID = 0x2A29;

// fwd declaration
static void assignRandomAddress();

static uint32_t LastTLMRCPacketMillis = 0;

static SemaphoreHandle_t _mutex = nullptr;

class BLEServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc  *param) override
  {
    if ( pServer != nullptr)
    {
        assignRandomAddress();
        NimBLEDevice::startAdvertising();
    }
  }
};
static BLEServerCB bleServerCB;

static String getMasterUIDString()
{
    char muids[7] = {0};
    sprintf(muids, "%02X%02X%02X", MasterUID[3], MasterUID[4], MasterUID[5]);
    return String(muids);
}

static void BluetoothTelemetrySendLinkStatsPacketEx(uint8_t* outBuffer)
{
    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[CRSF_TELEMETRY_LENGTH_INDEX] = CRSF_FRAME_SIZE(LinkStatisticsFrameLength);
    outBuffer[CRSF_TELEMETRY_TYPE_INDEX] = CRSF_FRAMETYPE_LINK_STATISTICS;
    outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1 + LinkStatisticsFrameLength] = crsf_crc.calc((byte *)&outBuffer[CRSF_TELEMETRY_TYPE_INDEX], LinkStatisticsFrameLength + 1);

    rcCRSF->setValue(outBuffer, LinkStatisticsFrameLength + 4);
    rcCRSF->notify();
}

static void BluetoothTelemetrySendEmptyLinkStatsPacket()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4];
    memset(&outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1], 0, LinkStatisticsFrameLength);

    crsfPayloadLinkstatistics_s* pStats = (crsfPayloadLinkstatistics_s*)(&outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1]);
    pStats->uplink_RSSI_1 = 120;
    pStats->uplink_RSSI_2 = 120;
    pStats->downlink_RSSI = 120;
    pStats->uplink_SNR = -20;
    pStats->downlink_SNR = -20;
    pStats->rf_Mode = CRSF::LinkStatistics.rf_Mode;
    pStats->uplink_TX_Power = CRSF::LinkStatistics.uplink_TX_Power;

    BluetoothTelemetrySendLinkStatsPacketEx(outBuffer);
}


static void BluetoothTelemetrySendLinkStatsPacket()
{
    if (!CRSF::CRSFstate)
    {
        return;
    }

    uint8_t outBuffer[LinkStatisticsFrameLength + 4];
    memcpy(&outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1], (byte *)&CRSF::LinkStatistics, LinkStatisticsFrameLength);
    //CRSF::LinkStatistics.uplink_RSSI_1 is negative number
    //we send negative numbers to handset. OpenTX handles it as signed byte.
    //Over BLE we send positive numbers, like inav/betaflight expects (unsigned byte, positive value = -dbm)
    crsfPayloadLinkstatistics_s* pStats = (crsfPayloadLinkstatistics_s*)(&outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1]);
    pStats->uplink_RSSI_1 = -pStats->uplink_RSSI_1;
    pStats->uplink_RSSI_2 = -pStats->uplink_RSSI_2;
    pStats->downlink_RSSI = -pStats->downlink_RSSI;
    BluetoothTelemetrySendLinkStatsPacketEx(outBuffer);
}

static void BluetoothTelemetrySendRCFrame()
{
//#ifdef BLE_SEND_CHANNELS_PACKET
    if (!CRSF::CRSFstate)
    {
        return;
    }

    uint8_t outBuffer[RCframeLength + 4];

    rcPacket_t* p = (rcPacket_t*)(&outBuffer[0]);

    p->header.device_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
    p->header.frame_size = RCframeLength + 2;
    p->header.type = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    p->channels.ch0 = ChannelData[0];
    p->channels.ch1 = ChannelData[1];
    p->channels.ch2 = ChannelData[2];
    p->channels.ch3 = ChannelData[3];
    p->channels.ch4 = ChannelData[4];
    p->channels.ch5 = ChannelData[5];
    p->channels.ch6 = ChannelData[6];
    p->channels.ch7 = ChannelData[7];
    p->channels.ch8 = ChannelData[8];
    p->channels.ch9 = ChannelData[9];
    p->channels.ch10 = ChannelData[10];
    p->channels.ch11 = ChannelData[11];
    p->channels.ch12 = ChannelData[12];
    p->channels.ch13 = ChannelData[13];
    p->channels.ch14 = ChannelData[14];
    p->channels.ch15 = ChannelData[15];

    outBuffer[CRSF_TELEMETRY_TYPE_INDEX + 1 + RCframeLength] = crsf_crc.calc((byte *)&outBuffer[CRSF_TELEMETRY_TYPE_INDEX], RCframeLength + 1 );

    rcCRSF->setValue(outBuffer, RCframeLength + 4);
    rcCRSF->notify();
//#endif
}

void BluetoothTelemetryShutdown()
{
    if ( pServer != nullptr)
    {
        DBGLN("Stopping BLE Telemetry!");

        pServer->setCallbacks(nullptr);
        pServer = nullptr;
        rcCRSF = nullptr;
        NimBLEDevice::stopAdvertising();
        NimBLEDevice::deinit(true);
        vSemaphoreDelete(_mutex);
    }
}

static void assignRandomAddress()
{
    uint8_t ble_address[6];

    memcpy( ble_address, NimBLEDevice::getAddress().getNative(), 6 );
    ble_address[5] |= 0xC0;  //random static address should have 11 at most significant bits
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    ble_hs_id_set_rnd(ble_address);
}

static void onAdvertisingStopped(NimBLEAdvertising *pAdv)
{
    if (pServer != nullptr)
    {
        assignRandomAddress();
        pAdv->start(0, &onAdvertisingStopped);
    }
}

static void BluetoothTelemetryUpdateDevice()
{
    if ( (config.GetBLETelemetry() == false) || (connectionState == bleJoystick) )
    {
        BluetoothTelemetryShutdown();
        return;
    }

    // pServer is null if it hasn't been started yet
    if (pServer != nullptr)
        return;

    _mutex = xSemaphoreCreateMutex();

    //NimBLEDevice::init(String(String(device_name) + " " + getMasterUIDString()).c_str());
    NimBLEDevice::init("ExpressLRS Telemetry");

    //we do not want devices which are bound to BLE Joystick to connect to Telemetry service.
    //start BLE Device with random address
    //avoid frequent address changes
    assignRandomAddress();

      //Set MTU to max packet length + 3 bytes to be able to send packets longer then default 20 bytes.
    //Should be set on both ends - smaller from two is used.
    NimBLEDevice::setMTU(CRSF_MAX_PACKET_LEN + 3);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&bleServerCB, true);
    NimBLEService *rcService = pServer->createService(TELEMETRY_SVC_UUID);
    rcCRSF = rcService->createCharacteristic(TELEMETRY_CRSF_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, CRSF_MAX_PACKET_LEN);
    rcService->start();

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

    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_DEFAULT);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_ADV);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL0);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL1);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL2);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL3);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL4);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL5);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL6);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL7);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6, ESP_BLE_PWR_TYPE_CONN_HDL8);

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(rcService->getUUID());

    pAdvertising->addServiceUUID(dInfo->getUUID());
    pAdvertising->start(0, &onAdvertisingStopped);

    DBGLN("Starting BLE Telemetry!");
}


static uint8_t _crsf_buffer[CRSF_MAX_PACKET_LEN] = {0u};
static int nextTimeLinkStats = 0;

int BluetoothTelemetryUpdateValues(const uint8_t *data)
{
    if (pServer == nullptr)
        return DURATION_NEVER;

    if (data != nullptr)
    {
        uint8_t size = data[CRSF_TELEMETRY_LENGTH_INDEX];
        if ( (size != 0u) && (CRSF_FRAME_SIZE(size)<=CRSF_MAX_PACKET_LEN) )
        {
          if ( (_mutex != nullptr) && (xSemaphoreTake( _mutex, ( TickType_t ) 10 ) == pdTRUE) )
          {
            memcpy(_crsf_buffer, data, CRSF_FRAME_SIZE(size));
            xSemaphoreGive( _mutex );
            devicesTriggerEvent(); // trigger event to send the telemetry
          }
        }
        return DURATION_IMMEDIATELY; // this value will be ignored becuase called from main here
    }

    if ( (_mutex != nullptr) && (xSemaphoreTake( _mutex, ( TickType_t ) 10 ) == pdTRUE) )
    {
        uint8_t size = _crsf_buffer[CRSF_TELEMETRY_LENGTH_INDEX];
        if ( (size != 0u) && (CRSF_FRAME_SIZE(size) <= CRSF_MAX_PACKET_LEN) )
        {
            rcCRSF->setValue(_crsf_buffer, CRSF_FRAME_SIZE(size));
            rcCRSF->notify();
            _crsf_buffer[CRSF_TELEMETRY_LENGTH_INDEX] = 0u;
        }
        xSemaphoreGive( _mutex );
    }

    uint32_t const now = millis();

    if (now >= (uint32_t)(LastTLMRCPacketMillis + BLE_CHANNELS_PACKET_PERIOD_MS))
    {
        /* Periodically send RC channels packet for Android Telemetry viewer */
        BluetoothTelemetrySendRCFrame();
        LastTLMRCPacketMillis = now;
    }

    if (nextTimeLinkStats <= now)
    {
        nextTimeLinkStats = nextTimeLinkStats + BLE_LINKSTATS_PACKET_PERIOD_MS; // - ((int)now-nextTimeLinkStats);
        if (nextTimeLinkStats <= (int)now) {
            nextTimeLinkStats = (int)now + BLE_LINKSTATS_PACKET_PERIOD_MS;
        }
        if (connectionState == connected)
        {
            BluetoothTelemetrySendLinkStatsPacket();
        }
        else
        {
            BluetoothTelemetrySendEmptyLinkStatsPacket();
        }
    }

    return nextTimeLinkStats-now;
}

static int event()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    BluetoothTelemetryUpdateDevice();
    return BluetoothTelemetryUpdateValues(nullptr);
}

device_t BLET_device = {
    .initialize = nullptr,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};
#endif
