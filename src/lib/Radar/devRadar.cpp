#include "devRadar.h"

// TODO: we need accurate time
// TODO: Need to validate all the fields going into the UAS structure
//   to be sure they are in range (whole section won't go if one value is outside the correct range)
// TODO: WGS-84 Operator alitude is required?

#if defined(USE_RADAR)
#include "logging.h"

#include <ESP8266WiFi.h>
#include "libopendroneid/opendroneid.h"

#define BEACON_REPORT_INTERVAL_MS 3000
#define BEACON_WIFI_CHANNEL       6
// A pack with 2x BasicID, 1xLoc, 1xSelfID, 1xSystem, 1xOperatorID ~223 bytes
#define WIFI_BUFF_SIZE 250

// State variables
static crsf_sensor_gps_t radar_Gps;
static struct tagOperatorLocation {
    bool isValid;
    double latitude;
    double longitude;
} OperatorLocation;

// User-supplied parameters
static RadarPhyMethod_e radar_PhyMethod;
// Operator ID (up to 20 chars)
static const char *radar_OperatorId = "FAA-OP-CAPNBRY1337";
// Drone ID (up to 20 chars)
static const char *radar_UasId = "CapnForceOne"; // FhpxNQvpxSNN";
// Transmitter serial number (4+1+12)
static const char *radar_serialId = "ELRSC123456789012";  // ANSI/CTA-2063-A

// Somewhat fixed parameters
static const ODID_uatype_t radar_UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR; // or ODID_UATYPE_AEROPLANE
static const char *radar_SelfId = "Recreational";

void Radar_UpdatePosition(const crsf_sensor_gps_t *gps)
{
    // The values are Big Endian, convert them to native byte order
    radar_Gps.latitude = be32toh(gps->latitude);
    radar_Gps.longitude = be32toh(gps->longitude);
    radar_Gps.groundspeed = be16toh(gps->groundspeed);
    radar_Gps.heading = be16toh(gps->heading);
    radar_Gps.altitude = be16toh(gps->altitude);
    radar_Gps.satellites = gps->satellites;

    // First time there are >5 sats, declare this operator position
    if (!OperatorLocation.isValid && radar_Gps.satellites > 5)
    {
        OperatorLocation.isValid = true;
        OperatorLocation.latitude = radar_Gps.latitude / 10000000.0;
        OperatorLocation.longitude = radar_Gps.longitude / 10000000.0;
    }
}

/***
 * @brief: Return a set of supported Phy Methods that can be used for transmission
 * @return: A bit set for each supproted method
 *          e.g. just rpmDisabled = 0x01
 *          e.g. both rpmDisabled and rpmWifiBeacon = 0x03
*/
uint32_t Radar_GetSupportedPhyMethods()
{
#if defined(PLATFORM_ESP8266)
    return bit(rpmDisabled) | bit(rpmWifiBeacon);
#elif defined(PLATFORM_ESP32)
    return bit(rpmDisabled) | bit(rpmWifiBeacon) | bit(rpmBluetoothLegacy);
#else
    return bit(rpmDisabled);
#endif
}

static int start()
{
    radar_PhyMethod = rpmWifiBeacon;

    // Start an AP with SSID=OperatorID, hidden, on the correct channel
    WiFi.softAP(radar_OperatorId, nullptr, BEACON_WIFI_CHANNEL, 1, 2, false);
    //WiFi.mode(WIFI_STA);
    //WiFi.setPhyMode(WIFI_PHY_MODE_11B);
    //wifi_set_phy_mode(PHY_MODE_11B);
    //esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

    // Output power
    //WiFi.setOutputPower(20);
    //system_phy_set_max_tpw(80);
    //esp_wifi_set_max_tx_power(80); // +20dBm

    // Some test data
    // radar_Gps.heading = 9000; // 90 degrees
    // radar_Gps.groundspeed = 880; // 88kph
    // radar_Gps.latitude = (int32_t)(28.085 * 10000000);
    // radar_Gps.longitude = (int32_t)(-82.641 * 10000000);
    // radar_Gps.altitude = 15 + 1000;
    // radar_Gps.satellites = 3;

    return BEACON_REPORT_INTERVAL_MS;
}

static void radar_FillUasData(ODID_UAS_Data &uasData)
{
    memset(&uasData, 0, sizeof(uasData));

    // Basic ID
    //odid_initBasicIDData(&uasData.BasicID[0]);
    uasData.BasicID[0].UAType = radar_UAType;
    uasData.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strcpy(uasData.BasicID[0].UASID, radar_serialId);
    uasData.BasicIDValid[0] = 1;
    //odid_initBasicIDData(&uasData.BasicID[1]);
    uasData.BasicID[1].UAType = radar_UAType;
    uasData.BasicID[1].IDType = ODID_IDTYPE_CAA_REGISTRATION_ID;
    strcpy(uasData.BasicID[1].UASID, radar_UasId);
    uasData.BasicIDValid[1] = 1;

    // Location
    //odid_initLocationData(&uasData.Location);
    uasData.Location.Status = ODID_STATUS_AIRBORNE; // CRSF::IsArmed() lollllls : ODID_STATUS_GROUND;
    uasData.Location.Direction = radar_Gps.heading / 100.0;
    uasData.Location.SpeedHorizontal = radar_Gps.groundspeed / 36.0;
    uasData.Location.SpeedVertical = INV_SPEED_V;
    uasData.Location.Latitude = radar_Gps.latitude / 10000000.0;
    uasData.Location.Longitude = radar_Gps.longitude / 10000000.0;
    uasData.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    uasData.Location.Height = radar_Gps.altitude - 1000.0;
    uasData.Location.AltitudeGeo  = INV_ALT;
    uasData.Location.AltitudeBaro = INV_ALT;
    uasData.Location.TimeStamp = INV_TIMESTAMP;
    uasData.LocationValid = 1;

    // Authdata
    // odid_initAuthData(&uasData.Auth[0]);
    // uasData.AuthValid[ODID_AUTH_MAX_PAGES] = 0;

    // SelfID
    //odid_initSelfIDData(&uasData.SelfID);
    uasData.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strcpy(uasData.SelfID.Desc, radar_SelfId);
    uasData.SelfIDValid = 1;

    // System
    //odid_initSystemData(&uasData.System)
    uasData.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    uasData.System.ClassificationType = ODID_CLASSIFICATION_TYPE_UNDECLARED;  // or EU?
    uasData.System.OperatorLatitude =  OperatorLocation.latitude;
    uasData.System.OperatorLongitude = OperatorLocation.longitude;
    uasData.System.AreaCount = 1;
    uasData.System.AreaRadius = 0;
    uasData.System.AreaCeiling = INV_ALT;
    uasData.System.AreaFloor = INV_ALT;
    //uasData.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
    //uasData.System.ClassEU = ODID_CLASS_EU_CLASS_4; // oh yeah I am totally a class 4 pilot, baby
    uasData.System.OperatorAltitudeGeo = INV_ALT;
    uasData.System.Timestamp = INV_TIMESTAMP;
    uasData.SystemValid = 1;

    // OperatorID
    //odid_initOperatorIDData(&uasData.OperatorID);
    strcpy(uasData.OperatorID.OperatorId, radar_OperatorId);
    uasData.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    uasData.OperatorIDValid = 1;
}

static void radar_sendBeacon()
{
    // Stuff that could be done in init
    uint8_t wifimac[6];
    wifi_get_macaddr(SOFTAP_IF, wifimac);
    //wifi_get_macaddr(STATION_IF, wifimac);

    ODID_UAS_Data uasData;
    radar_FillUasData(uasData);

    static uint8_t counter;
    uint8_t buf[WIFI_BUFF_SIZE];

    int packedLen = 0;
    if (radar_PhyMethod == rpmWifiBeacon)
    {
        packedLen = odid_wifi_build_message_pack_beacon_frame(
            &uasData, (char *)wifimac,
            radar_OperatorId, strlen(radar_OperatorId), BEACON_REPORT_INTERVAL_MS,
            counter, buf, WIFI_BUFF_SIZE);
    }
    else if (radar_PhyMethod == rpmWifiNan)
    {
        packedLen = odid_wifi_build_message_pack_nan_action_frame(
            &uasData, (char *)wifimac,
            counter, buf, WIFI_BUFF_SIZE);
    }

    if (packedLen > 0)
    {
        ++counter;
        int err = wifi_send_pkt_freedom(buf, packedLen, true);
        DBGLN("wifi_send_pkt_freedom=%d", err);

        // ESP32
        //esp_wifi_80211_tx(WIFI_IF_AP, buf, packedLen, true);
    }
}

static int event()
{
    return DURATION_IGNORE;
}

static int timeout()
{
    // Stop all transmitting if in wifi mode, update mode, etc
    if (connectionState > MODE_STATES || radar_PhyMethod == rpmDisabled)
    {
        return DURATION_NEVER;
    }

    radar_sendBeacon();

    return BEACON_REPORT_INTERVAL_MS;
}

device_t Radar_device = {
    .initialize = nullptr,
    .start = start,
    .event = event,
    .timeout = timeout,
};

#endif // USE_RADAR