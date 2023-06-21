#include "devRadar.h"

#if defined(USE_RADAR)

#include "user_interface.h"
#include "logging.h"
#include "libopendroneid/opendroneid.h"
#include "libopendroneid/odid_wifi.h"

#if defined(PLATFORM_ESP8266)

typedef struct {
	signed rssi: 8;
	unsigned rate: 4;
	unsigned is_group: 1;
	unsigned: 1;
	unsigned sig_mode: 2;
	unsigned legacy_length: 12;
	unsigned damatch0: 1;
	unsigned damatch1: 1;
	unsigned bssidmatch0: 1;
	unsigned bssidmatch1: 1;
	unsigned MCS: 7;
	unsigned CWB: 1;
	unsigned HT_length: 16;
	unsigned Smoothing: 1;
	unsigned Not_Sounding: 1;
	unsigned: 1;
	unsigned Aggregation: 1;
	unsigned STBC: 2;
	unsigned FEC_CODING: 1;
	unsigned SGI: 1;
	unsigned rxend_state: 8;
	unsigned ampdu_cnt: 8;
	unsigned channel: 4;
	unsigned: 12;
} wifi_pkt_rx_ctrl_t;

typedef struct {
	wifi_pkt_rx_ctrl_t rx_ctrl;
	uint8_t payload[0]; /* ieee80211 packet buff */
} wifi_promiscuous_pkt_t;

#endif // PLATFORM_ESP8266

#define IEEE80211_ELEMID_SSID		0x00
#define IEEE80211_ELEMID_RATES		0x01
#define IEEE80211_ELEMID_VENDOR		0xDD

typedef struct tagRadarPilotInfo {
    uint8_t mac[6]; // because we can't rely on users to not fly two things with the same OID
    char operator_id[RADAR_ID_LEN+1];
    int8_t rssi;         // dBm
    uint32_t last_seen;  // ms
    int32_t latitude;    // in * 10M
    int32_t longitude;   // in * 10M
    int16_t altitude;    // in m
} RadarPilotInfo;

static void wifi_promiscuous_cb(uint8 *buf, uint16 len)
{
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    ieee80211_mgmt *hdr = (ieee80211_mgmt *)pkt->payload;

    // Looking for a beacon frame, 0x8000
    // MAC of transmitter is in hdr.sa
    if (hdr->frame_control != htons(0x8000))
        return;

    RadarPilotInfo pilot;
    size_t payloadlen = len - sizeof(wifi_pkt_rx_ctrl_t);
    size_t offset = sizeof(ieee80211_mgmt) + sizeof(ieee80211_beacon);
    bool found_odid = false;
    while (offset < payloadlen && !found_odid)
    {
        switch (pkt->payload[offset]) // ieee80211 element_id
        {
        case IEEE80211_ELEMID_SSID:
            {
                // Operator ID is in the SSID field
                ieee80211_ssid *iessid = (ieee80211_ssid *)&pkt->payload[offset];
                uint8_t ssid_len = std::min((uint8_t)RADAR_ID_LEN, iessid->length);
                memcpy(pilot.operator_id, iessid->ssid, ssid_len);
                pilot.operator_id[ssid_len] = '\0';
                offset += sizeof(ieee80211_ssid) + iessid->length;
                break;
            }
        case IEEE80211_ELEMID_RATES:
            {
                ieee80211_supported_rates *iesr = (ieee80211_supported_rates *)&pkt->payload[offset];
                if (iesr->length > 1 || iesr->supported_rates != 0x8C)  // ODID beacon is 6 mbit
                    return;
                offset += sizeof(ieee80211_supported_rates);
                break;
            }
        case IEEE80211_ELEMID_VENDOR:
            {
                ieee80211_vendor_specific *ievs = (ieee80211_vendor_specific *)&pkt->payload[offset];
                // The OUI of opendroneid is FA-0B-BC
                if (ievs->oui[0] != 0xfa || ievs->oui[1] != 0x0b || ievs->oui[2] != 0xbc || ievs->oui_type != 0x0d)
                    return;
                offset += sizeof(ieee80211_vendor_specific);
                found_odid = true;
                break;
            }
        default:
            // Any unexpected element_id means we can pound sand
            return;
        } // switch
    } // while (<len)

    if (!found_odid)
        return;

    //DBGLN("Found ODID: %s", pilot.operator_id);

    pilot.rssi = pkt->rx_ctrl.rssi;
    pilot.last_seen = millis();
    memcpy(pilot.mac, hdr->sa, sizeof(pilot.mac));

    //ODID_service_info *si = (struct ODID_service_info *)&pkt->payload[offset];
    // can use si->counter to maybe do link quality, by detecting missing packet
    offset += sizeof(ODID_service_info);

    // Espressif are assholes for not giving us the whole packet, can't decode it because it is short
    ODID_MessagePack_encoded *msg_pack_enc = (ODID_MessagePack_encoded *)&pkt->payload[offset];
    offset += 3; // Version/MessageType SingleMessageSize MsgPackSize
    //size_t expected_msg_size = sizeof(*msg_pack_enc) - ODID_MESSAGE_SIZE * (ODID_PACK_MAX_MESSAGES - msg_pack_enc->MsgPackSize);
    //DBGLN("Found ODID: %s (%d dBm)", pilot.operator_id, pilot.rssi);
    for (unsigned i=0; i<msg_pack_enc->MsgPackSize && (offset+ODID_MESSAGE_SIZE)<payloadlen; ++i)
    {
        uint8_t MessageType = decodeMessageType(msg_pack_enc->Messages[i].rawData[0]);
        if (MessageType == ODID_MESSAGETYPE_LOCATION)
        {
            ODID_Location_encoded *loc = (ODID_Location_encoded *)&msg_pack_enc->Messages[i];
            // Conveniently, lat/lon are already 10M scale integer which is our format
            pilot.latitude = loc->Latitude;
            pilot.longitude = loc->Longitude;
            // Altitude is encoded 2x scale + 1000
            pilot.altitude = loc->Height / 2 - 1000;
            DBGLN("%d %d %d", pilot.latitude, pilot.longitude, pilot.altitude);
            //return;
        }
        offset += msg_pack_enc->SingleMessageSize;
        // off=94 len=116, means there's 22 of 25 bytes in msg[1]
        //DBGLN("off=%u len=%u", offset, payloadlen);
    }
}

void RadarRx_Begin()
{
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
    wifi_promiscuous_enable(1);
}

void RadarRx_End()
{
    wifi_promiscuous_enable(0);
    //wifi_set_promiscuous_rx_cb(nullptr);
}

#endif