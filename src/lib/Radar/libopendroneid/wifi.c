/*
Copyright (C) 2020 Simon Wunderlich, Marek Sobe
Copyright (C) 2020 Doodle Labs

SPDX-License-Identifier: Apache-2.0

Open Drone ID C Library

Maintainer:
Simon Wunderlich
sw@simonwunderlich.de
*/

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
int clock_gettime(clockid_t, struct timespec *);
#else
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#endif

#include <errno.h>
#include <time.h>

#include "opendroneid.h"
#include "odid_wifi.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define cpu_to_le16(x)  (x)
#define cpu_to_le64(x)  (x)
#else
#define cpu_to_le16(x)      (bswap_16(x))
#define cpu_to_le64(x)      (bswap_64(x))
#endif

#define IEEE80211_FCTL_FTYPE          0x000c
#define IEEE80211_FCTL_STYPE          0x00f0

#define IEEE80211_FTYPE_MGMT            0x0000
#define IEEE80211_STYPE_ACTION          0x00D0
#define IEEE80211_STYPE_BEACON          0x0080

/* IEEE 802.11-2016 capability info */
#define IEEE80211_CAPINFO_ESS               0x0001
#define IEEE80211_CAPINFO_IBSS              0x0002
#define IEEE80211_CAPINFO_CF_POLLABLE       0x0004
#define IEEE80211_CAPINFO_CF_POLLREQ        0x0008
#define IEEE80211_CAPINFO_PRIVACY           0x0010
#define IEEE80211_CAPINFO_SHORT_PREAMBLE    0x0020
/* bits 6-7 reserved */
#define IEEE80211_CAPINFO_SPECTRUM_MGMT     0x0100
#define IEEE80211_CAPINFO_QOS               0x0200
#define IEEE80211_CAPINFO_SHORT_SLOTTIME    0x0400
#define IEEE80211_CAPINFO_APSD              0x0800
#define IEEE80211_CAPINFO_RADIOMEAS         0x1000
/* bit 13 reserved */
#define IEEE80211_CAPINFO_DEL_BLOCK_ACK     0x4000
#define IEEE80211_CAPINFO_IMM_BLOCK_ACK     0x8000

/* IEEE 802.11 Element IDs */
#define IEEE80211_ELEMID_SSID		0x00
#define IEEE80211_ELEMID_RATES		0x01
#define IEEE80211_ELEMID_VENDOR		0xDD

/* Neighbor Awareness Networking Specification v3.1 in section 2.8.2
 * The NAN Cluster ID is a MAC address that takes a value from
 * 50-6F-9A-01-00-00 to 50-6F-9A-01-FF-FF and is carried in the A3 field of
 * some of the NAN frames. The NAN Cluster ID is randomly chosen by the device
 * that initiates the NAN Cluster.
 * However, the ASTM Remote ID specification v1.1 specifies that the NAN
 * cluster ID must be fixed to the value 50-6F-9A-01-00-FF.
 */
static const uint8_t *get_nan_cluster_id(void)
{
    static const uint8_t cluster_id[6] = { 0x50, 0x6F, 0x9A, 0x01, 0x00, 0xFF };
    return cluster_id;
}

static int buf_fill_ieee80211_mgmt(uint8_t *buf, size_t *len, size_t buf_size,
                                   const uint16_t subtype,
                                   const uint8_t *dst_addr,
                                   const uint8_t *src_addr,
                                   const uint8_t *bssid)
{
    if (*len + sizeof(struct ieee80211_mgmt) > buf_size)
        return -ENOMEM;

    struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)(buf + *len);
    mgmt->frame_control = (uint16_t) cpu_to_le16(IEEE80211_FTYPE_MGMT | subtype);
    mgmt->duration = cpu_to_le16(0x0000);
    memcpy(mgmt->da, dst_addr, sizeof(mgmt->da));
    memcpy(mgmt->sa, src_addr, sizeof(mgmt->sa));
    memcpy(mgmt->bssid, bssid, sizeof(mgmt->bssid));
    mgmt->seq_ctrl = cpu_to_le16(0x0000);
    *len += sizeof(*mgmt);

    return 0;
}

static int buf_fill_ieee80211_beacon(uint8_t *buf, size_t *len, size_t buf_size, uint16_t interval_tu)
{
    if (*len + sizeof(struct ieee80211_beacon) > buf_size)
        return -ENOMEM;

    struct ieee80211_beacon *beacon = (struct ieee80211_beacon *)(buf + *len);
    struct timespec ts;
    uint64_t mono_us = 0;

#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
    mono_us = (uint64_t)((double) ts.tv_sec * 1e6 + (double) ts.tv_nsec * 1e-3);
#elif defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
    mono_us = (uint64_t)((double) ts.tv_sec * 1e6 + (double) ts.tv_nsec * 1e-3);
#elif defined(ARDUINO)
#warning "No REALTIME or MONOTONIC clock, using micros()."
    mono_us = micros();
#else
#warning "Unable to set wifi timestamp."
#endif
    beacon->timestamp = cpu_to_le64(mono_us);
    beacon->beacon_interval = cpu_to_le16(interval_tu);
    beacon->capability = cpu_to_le16(IEEE80211_CAPINFO_SHORT_SLOTTIME | IEEE80211_CAPINFO_SHORT_PREAMBLE);
    *len += sizeof(*beacon);

    return 0;
}

void drone_export_gps_data(ODID_UAS_Data *UAS_Data, char *buf, size_t buf_size)
{
    ptrdiff_t len = 0;

#define mprintf(...) {\
    len += snprintf(buf + len, buf_size - (size_t)len, __VA_ARGS__); \
    if ((len < 0) || ((size_t)len >= buf_size)) \
        return; \
    }

    mprintf("{\n\t\"Version\": \"1.1\",\n\t\"Response\": {\n");

    mprintf("\t\t\"BasicID\": {\n");
    for (int i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
        if (!UAS_Data->BasicIDValid[i])
            continue;
        mprintf("\t\t\t\"UAType%d\": %d,\n", i, UAS_Data->BasicID[i].UAType);
        mprintf("\t\t\t\"IDType%d\": %d,\n", i, UAS_Data->BasicID[i].IDType);
        mprintf("\t\t\t\"UASID%d\": %s,\n", i, UAS_Data->BasicID[i].UASID);
    }
    mprintf("\t\t},\n");

    mprintf("\t\t\"Location\": {\n");
    mprintf("\t\t\t\"Status\": %d,\n", (int)UAS_Data->Location.Status);
    mprintf("\t\t\t\"Direction\": %f,\n", (double) UAS_Data->Location.Direction);
    mprintf("\t\t\t\"SpeedHorizontal\": %f,\n", (double) UAS_Data->Location.SpeedHorizontal);
    mprintf("\t\t\t\"SpeedVertical\": %f,\n", (double) UAS_Data->Location.SpeedVertical);
    mprintf("\t\t\t\"Latitude\": %f,\n", UAS_Data->Location.Latitude);
    mprintf("\t\t\t\"Longitude\": %f,\n", UAS_Data->Location.Longitude);
    mprintf("\t\t\t\"AltitudeBaro\": %f,\n", (double) UAS_Data->Location.AltitudeBaro);
    mprintf("\t\t\t\"AltitudeGeo\": %f,\n", (double) UAS_Data->Location.AltitudeGeo);
    mprintf("\t\t\t\"HeightType\": %d,\n", UAS_Data->Location.HeightType);
    mprintf("\t\t\t\"Height\": %f,\n", (double) UAS_Data->Location.Height);
    mprintf("\t\t\t\"HorizAccuracy\": %d,\n", UAS_Data->Location.HorizAccuracy);
    mprintf("\t\t\t\"VertAccuracy\": %d,\n", UAS_Data->Location.VertAccuracy);
    mprintf("\t\t\t\"BaroAccuracy\": %d,\n", UAS_Data->Location.BaroAccuracy);
    mprintf("\t\t\t\"SpeedAccuracy\": %d,\n", UAS_Data->Location.SpeedAccuracy);
    mprintf("\t\t\t\"TSAccuracy\": %d,\n", UAS_Data->Location.TSAccuracy);
    mprintf("\t\t\t\"TimeStamp\": %f,\n", (double) UAS_Data->Location.TimeStamp);
    mprintf("\t\t},\n");

    mprintf("\t\t\"Authentication\": {\n");
    mprintf("\t\t\t\"AuthType\": %d,\n", UAS_Data->Auth[0].AuthType);
    mprintf("\t\t\t\"LastPageIndex\": %d,\n", UAS_Data->Auth[0].LastPageIndex);
    mprintf("\t\t\t\"Length\": %d,\n", UAS_Data->Auth[0].Length);
    mprintf("\t\t\t\"Timestamp\": %u,\n", UAS_Data->Auth[0].Timestamp);
    for (int i = 0; i <= UAS_Data->Auth[0].LastPageIndex; i++) {
        mprintf("\t\t\t\"AuthData Page %d,\": %s\n", i, UAS_Data->Auth[i].AuthData);
    }
    mprintf("\t\t},\n");

    mprintf("\t\t\"SelfID\": {\n");
    mprintf("\t\t\t\"Description Type\": %d,\n", UAS_Data->SelfID.DescType);
    mprintf("\t\t\t\"Description\": %s,\n", UAS_Data->SelfID.Desc);
    mprintf("\t\t},\n");

    mprintf("\t\t\"Operator\": {\n");
    mprintf("\t\t\t\"OperatorLocationType\": %d,\n", UAS_Data->System.OperatorLocationType);
    mprintf("\t\t\t\"ClassificationType\": %d,\n", UAS_Data->System.ClassificationType);
    mprintf("\t\t\t\"OperatorLatitude\": %f,\n", UAS_Data->System.OperatorLatitude);
    mprintf("\t\t\t\"OperatorLongitude\": %f,\n", UAS_Data->System.OperatorLongitude);
    mprintf("\t\t\t\"AreaCount\": %d,\n", UAS_Data->System.AreaCount);
    mprintf("\t\t\t\"AreaRadius\": %d,\n", UAS_Data->System.AreaRadius);
    mprintf("\t\t\t\"AreaCeiling\": %f,\n", (double) UAS_Data->System.AreaCeiling);
    mprintf("\t\t\t\"AreaFloor\": %f,\n", (double) UAS_Data->System.AreaFloor);
    mprintf("\t\t\t\"CategoryEU\": %d,\n", UAS_Data->System.CategoryEU);
    mprintf("\t\t\t\"ClassEU\": %d,\n", UAS_Data->System.ClassEU);
    mprintf("\t\t\t\"OperatorAltitudeGeo\": %f,\n", (double) UAS_Data->System.OperatorAltitudeGeo);
    mprintf("\t\t\t\"Timestamp\": %u,\n", UAS_Data->System.Timestamp);
    mprintf("\t\t}\n");

    mprintf("\t\t\"OperatorID\": {\n");
    mprintf("\t\t\t\"OperatorIdType\": %d,\n", UAS_Data->OperatorID.OperatorIdType);
    mprintf("\t\t\t\"OperatorId\": \"%s\",\n", UAS_Data->OperatorID.OperatorId);
    mprintf("\t\t},\n");

    mprintf("\t}\n}");
}

int odid_message_build_pack(ODID_UAS_Data *UAS_Data, void *pack, size_t buflen)
{
    ODID_MessagePack_data msg_pack;
    ODID_MessagePack_encoded *msg_pack_enc;
    size_t len;

    /* create a complete message pack */
    msg_pack.SingleMessageSize = ODID_MESSAGE_SIZE;
    msg_pack.MsgPackSize = 0;
    for (int i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
        if (UAS_Data->BasicIDValid[i]) {
            if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
                return -EINVAL;
            if (encodeBasicIDMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->BasicID[i]) == ODID_SUCCESS)
                msg_pack.MsgPackSize++;
        }
    }
    if (UAS_Data->LocationValid) {
        if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
            return -EINVAL;
        if (encodeLocationMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->Location) == ODID_SUCCESS)
            msg_pack.MsgPackSize++;
    }
    for (int i = 0; i < ODID_AUTH_MAX_PAGES; i++)
    {
        if (UAS_Data->AuthValid[i]) {
            if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
                return -EINVAL;
            if (encodeAuthMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->Auth[i]) == ODID_SUCCESS)
                msg_pack.MsgPackSize++;
        }
    }
    if (UAS_Data->SelfIDValid) {
        if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
            return -EINVAL;
        if (encodeSelfIDMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->SelfID) == ODID_SUCCESS)
            msg_pack.MsgPackSize++;
    }
    if (UAS_Data->SystemValid) {
        if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
            return -EINVAL;
        if (encodeSystemMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->System) == ODID_SUCCESS)
            msg_pack.MsgPackSize++;
    }
    if (UAS_Data->OperatorIDValid) {
        if (msg_pack.MsgPackSize >= ODID_PACK_MAX_MESSAGES)
            return -EINVAL;
        if (encodeOperatorIDMessage((void *)&msg_pack.Messages[msg_pack.MsgPackSize], &UAS_Data->OperatorID) == ODID_SUCCESS)
            msg_pack.MsgPackSize++;
    }

    /* check that there is at least one message to send. */
    if (msg_pack.MsgPackSize == 0)
        return -EINVAL;

    /* calculate the exact encoded message pack size. */
    len = sizeof(*msg_pack_enc) - (ODID_PACK_MAX_MESSAGES - msg_pack.MsgPackSize) * ODID_MESSAGE_SIZE;

    /* check if there is enough space for the message pack. */
    if (len > buflen)
        return -ENOMEM;

    msg_pack_enc = (ODID_MessagePack_encoded *) pack;
    if (encodeMessagePack(msg_pack_enc, &msg_pack) != ODID_SUCCESS)
        return -1;

    return (int) len;
}

int odid_wifi_build_nan_sync_beacon_frame(char *mac, uint8_t *buf, size_t buf_size)
{
    /* Broadcast address */
    uint8_t target_addr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t wifi_alliance_oui[3] = { 0x50, 0x6F, 0x9A };
    /* "org.opendroneid.remoteid" hash */
    uint8_t service_id[6] = { 0x88, 0x69, 0x19, 0x9D, 0x92, 0x09 };
    const uint8_t *cluster_id = get_nan_cluster_id();
    struct ieee80211_vendor_specific *vendor;
    struct nan_master_indication_attribute *master_indication_attr;
    struct nan_cluster_attribute *cluster_attr;
    struct nan_service_id_list_attribute *nsila;
    int ret;
    size_t len = 0;

    /* IEEE 802.11 Management Header */
    ret = buf_fill_ieee80211_mgmt(buf, &len, buf_size, IEEE80211_STYPE_BEACON, target_addr, (uint8_t *)mac, cluster_id);
    if (ret <0)
        return ret;

    /* Beacon */
    ret = buf_fill_ieee80211_beacon(buf, &len, buf_size, 0x0200);
    if (ret <0)
        return ret;

    /* Vendor Specific */
    if (len + sizeof(*vendor) > buf_size)
        return -ENOMEM;

    vendor = (struct ieee80211_vendor_specific *)(buf + len);
    memset(vendor, 0, sizeof(*vendor));
    vendor->element_id = IEEE80211_ELEMID_VENDOR;
    vendor->length = 0x22;
    memcpy(vendor->oui, wifi_alliance_oui, sizeof(vendor->oui));
    vendor->oui_type = 0x13;
    len += sizeof(*vendor);

    /* NAN Master Indication attribute */
    if (len + sizeof(*master_indication_attr) > buf_size)
        return -ENOMEM;

    master_indication_attr = (struct nan_master_indication_attribute *)(buf + len);
    memset(master_indication_attr, 0, sizeof(*master_indication_attr));
    master_indication_attr->header.attribute_id = 0x00;
    master_indication_attr->header.length = cpu_to_le16(0x0002);
    /* Information that is used to indicate a NAN Device’s preference to serve
     * as the role of Master, with a larger value indicating a higher
     * preference. Values 1 and 255 are used for testing purposes only.
     */
    master_indication_attr->master_preference = 0xFE;
    /* Random factor value 0xEA is recommended by the European Standard */
    master_indication_attr->random_factor = 0xEA;
    len += sizeof(*master_indication_attr);

    /* NAN Cluster attribute */
    if (len + sizeof(*cluster_attr) > buf_size)
        return -ENOMEM;

    cluster_attr = (struct nan_cluster_attribute *)(buf + len);
    memset(cluster_attr, 0, sizeof(*cluster_attr));
    cluster_attr->header.attribute_id = 0x1;
    cluster_attr->header.length = cpu_to_le16(0x000D);
    memcpy(cluster_attr->device_mac, mac, sizeof(cluster_attr->device_mac));
    cluster_attr->random_factor = 0xEA;
    cluster_attr->master_preference = 0xFE;
    cluster_attr->hop_count_to_anchor_master = 0x00;
    memset(cluster_attr->anchor_master_beacon_transmission_time, 0, sizeof(cluster_attr->anchor_master_beacon_transmission_time));
    len += sizeof(*cluster_attr);

    /* NAN attributes */
    if (len + sizeof(*nsila) > buf_size)
        return -ENOMEM;

    nsila = (struct nan_service_id_list_attribute *)(buf + len);
    memset(nsila, 0, sizeof(*nsila));
    nsila->header.attribute_id = 0x02;
    nsila->header.length = cpu_to_le16(0x0006);
    memcpy(nsila->service_id, service_id, sizeof(service_id));
    len += sizeof(*nsila);

    return (int) len;
}

int odid_wifi_build_message_pack_nan_action_frame(ODID_UAS_Data *UAS_Data, char *mac,
                                                  uint8_t send_counter,
                                                  uint8_t *buf, size_t buf_size)
{
    /* Neighbor Awareness Networking Specification v3.0 in section 2.8.1
     * NAN Network ID calls for the destination mac to be 51-6F-9A-01-00-00 */
    uint8_t target_addr[6] = { 0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00 };
    /* "org.opendroneid.remoteid" hash */
    uint8_t service_id[6] = { 0x88, 0x69, 0x19, 0x9D, 0x92, 0x09 };
    uint8_t wifi_alliance_oui[3] = { 0x50, 0x6F, 0x9A };
    const uint8_t *cluster_id = get_nan_cluster_id();
    struct nan_service_discovery *nsd;
    struct nan_service_descriptor_attribute *nsda;
    struct nan_service_descriptor_extension_attribute *nsdea;
    struct ODID_service_info *si;
    int ret;
    size_t len = 0;

    /* IEEE 802.11 Management Header */
    ret = buf_fill_ieee80211_mgmt(buf, &len, buf_size, IEEE80211_STYPE_ACTION, target_addr, (uint8_t *)mac, cluster_id);
    if (ret <0)
        return ret;

    /* NAN Service Discovery header */
    if (len + sizeof(*nsd) > buf_size)
        return -ENOMEM;

    nsd = (struct nan_service_discovery *)(buf + len);
    memset(nsd, 0, sizeof(*nsd));
    nsd->category = 0x04;               /* IEEE 802.11 Public Action frame */
    nsd->action_code = 0x09;            /* IEEE 802.11 Public Action frame Vendor Specific*/
    memcpy(nsd->oui, wifi_alliance_oui, sizeof(nsd->oui));
    nsd->oui_type = 0x13;               /* Identify Type and version of the NAN */
    len += sizeof(*nsd);

    /* NAN Attribute for Service Descriptor header */
    if (len + sizeof(*nsda) > buf_size)
        return -ENOMEM;

    nsda = (struct nan_service_descriptor_attribute *)(buf + len);
    nsda->header.attribute_id = 0x3;    /* Service Descriptor Attribute type */
    memcpy(nsda->service_id, service_id, sizeof(service_id));
    /* always 1 */
    nsda->instance_id = 0x01;           /* always 1 */
    nsda->requestor_instance_id = 0x00; /* from triggering frame */
    nsda->service_control = 0x10;       /* follow up */
    len += sizeof(*nsda);

    /* ODID Service Info Attribute header */
    if (len + sizeof(*si) > buf_size)
        return -ENOMEM;

    si = (struct ODID_service_info *)(buf + len);
    memset(si, 0, sizeof(*si));
    si->message_counter = send_counter;
    len += sizeof(*si);

    ret = odid_message_build_pack(UAS_Data, buf + len, buf_size - len);
    if (ret < 0)
        return ret;
    len += ret;

    /* set the lengths according to the message pack lengths */
    nsda->service_info_length = sizeof(*si) + ret;
    nsda->header.length = cpu_to_le16(sizeof(*nsda) - sizeof(struct nan_attribute_header) + nsda->service_info_length);

    /* NAN Attribute for Service Descriptor extension header */
    if (len + sizeof(*nsdea) > buf_size)
        return -ENOMEM;

    nsdea = (struct nan_service_descriptor_extension_attribute *)(buf + len);
    nsdea->header.attribute_id = 0xE;
    nsdea->header.length = cpu_to_le16(0x0004);
    nsdea->instance_id = 0x01;
    nsdea->control = cpu_to_le16(0x0200);
    nsdea->service_update_indicator = send_counter;
    len += sizeof(*nsdea);

    return (int) len;
}

int odid_wifi_build_message_pack_beacon_frame(ODID_UAS_Data *UAS_Data, char *mac,
                                              const char *SSID, size_t SSID_len,
                                              uint16_t interval_tu, uint8_t send_counter,
                                              uint8_t *buf, size_t buf_size)
{
    /* Broadcast address */
    uint8_t target_addr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t asd_stan_oui[3] = { 0xFA, 0x0B, 0xBC };
    /* Mgmt Beacon frame mandatory fields + IE 221 */
    struct ieee80211_ssid *ssid_s;
    struct ieee80211_supported_rates *rates;
    struct ieee80211_vendor_specific *vendor;

    /* Message Pack */
    struct ODID_service_info *si;

    int ret;
    size_t len = 0;

    /* IEEE 802.11 Management Header */
    ret = buf_fill_ieee80211_mgmt(buf, &len, buf_size, IEEE80211_STYPE_BEACON, target_addr, (uint8_t *)mac, (uint8_t *)mac);
    if (ret <0)
        return ret;

    /* Mandatory Beacon as of 802.11-2016 Part 11 */
    ret = buf_fill_ieee80211_beacon(buf, &len, buf_size, interval_tu);
    if (ret <0)
        return ret;

    /* SSID: 1-32 bytes */
    if (len + sizeof(*ssid_s) > buf_size)
        return -ENOMEM;

    ssid_s = (struct ieee80211_ssid *)(buf + len);
    if(!SSID || (SSID_len ==0) || (SSID_len > 32))
        return -EINVAL;
    ssid_s->element_id = IEEE80211_ELEMID_SSID;
    ssid_s->length = (uint8_t) SSID_len;
    memcpy(ssid_s->ssid, SSID, ssid_s->length);
    len += sizeof(*ssid_s) + SSID_len;

    /* Supported Rates: 1 record at minimum */
    if (len + sizeof(*rates) > buf_size)
        return -ENOMEM;

    rates = (struct ieee80211_supported_rates *)(buf + len);
    rates->element_id = IEEE80211_ELEMID_RATES;
    rates->length = 1; // One rate only
    rates->supported_rates = 0x8C;     // 6 Mbps
    len += sizeof(*rates);

    /* Vendor Specific Information Element (IE 221) */
    if (len + sizeof(*vendor) > buf_size)
        return -ENOMEM;

    vendor = (struct ieee80211_vendor_specific *)(buf + len);
    vendor->element_id = IEEE80211_ELEMID_VENDOR;
    vendor->length = 0x00;  // Length updated at end of function
    memcpy(vendor->oui, asd_stan_oui, sizeof(vendor->oui));
    vendor->oui_type = 0x0D;
    len += sizeof(*vendor);

    /* ODID Service Info Attribute header */
    if (len + sizeof(*si) > buf_size)
        return -ENOMEM;

    si = (struct ODID_service_info *)(buf + len);
    memset(si, 0, sizeof(*si));
    si->message_counter = send_counter;
    len += sizeof(*si);

    ret = odid_message_build_pack(UAS_Data, buf + len, buf_size - len);
    if (ret < 0)
        return ret;
    len += ret;

    /* set the lengths according to the message pack lengths */
    vendor->length = sizeof(vendor->oui) + sizeof(vendor->oui_type) + sizeof(*si) + ret;

    return (int) len;
}

int odid_message_process_pack(ODID_UAS_Data *UAS_Data, uint8_t *pack, size_t buflen)
{
    ODID_MessagePack_encoded *msg_pack_enc = (ODID_MessagePack_encoded *) pack;
    size_t size = sizeof(*msg_pack_enc) - ODID_MESSAGE_SIZE * (ODID_PACK_MAX_MESSAGES - msg_pack_enc->MsgPackSize);
    if (size > buflen)
        return -ENOMEM;

    odid_initUasData(UAS_Data);

    if (decodeMessagePack(UAS_Data, msg_pack_enc) != ODID_SUCCESS)
        return -1;

    return (int) size;
}

int odid_wifi_receive_message_pack_nan_action_frame(ODID_UAS_Data *UAS_Data,
                                                    char *mac, uint8_t *buf, size_t buf_size)
{
    struct ieee80211_mgmt *mgmt;
    struct nan_service_discovery *nsd;
    struct nan_service_descriptor_attribute *nsda;
    struct nan_service_descriptor_extension_attribute *nsdea;
    struct ODID_service_info *si;
    uint8_t target_addr[6] = { 0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00 };
    uint8_t wifi_alliance_oui[3] = { 0x50, 0x6F, 0x9A };
    uint8_t service_id[6] = { 0x88, 0x69, 0x19, 0x9D, 0x92, 0x09 };
    int ret;
    size_t len = 0;

    /* IEEE 802.11 Management Header */
    if (len + sizeof(*mgmt) > buf_size)
        return -EINVAL;
    mgmt = (struct ieee80211_mgmt *)(buf + len);
    if ((mgmt->frame_control & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) !=
        cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION))
        return -EINVAL;
    if (memcmp(mgmt->da, target_addr, sizeof(mgmt->da)) != 0)
        return -EINVAL;
    memcpy(mac, mgmt->sa, sizeof(mgmt->sa));

    len += sizeof(*mgmt);

    /* NAN Service Discovery header */
    if (len + sizeof(*nsd) > buf_size)
        return -EINVAL;
    nsd = (struct nan_service_discovery *)(buf + len);
    if (nsd->category != 0x04)
        return -EINVAL;
    if (nsd->action_code != 0x09)
        return -EINVAL;
    if (memcmp(nsd->oui, wifi_alliance_oui, sizeof(wifi_alliance_oui)) != 0)
        return -EINVAL;
    if (nsd->oui_type != 0x13)
        return -EINVAL;
    len += sizeof(*nsd);

    /* NAN Attribute for Service Descriptor header */
    if (len + sizeof(*nsda) > buf_size)
        return -EINVAL;
    nsda = (struct nan_service_descriptor_attribute *)(buf + len);
    if (nsda->header.attribute_id != 0x3)
        return -EINVAL;
    if (memcmp(nsda->service_id, service_id, sizeof(service_id)) != 0)
        return -EINVAL;
    if (nsda->instance_id != 0x01)
        return -EINVAL;
    if (nsda->service_control != 0x10)
        return -EINVAL;
    len += sizeof(*nsda);

    si = (struct ODID_service_info *)(buf + len);
    ret = odid_message_process_pack(UAS_Data, buf + len + sizeof(*si), buf_size - len - sizeof(*nsdea));
    if (ret < 0)
        return -EINVAL;
    if (nsda->service_info_length != (sizeof(*si) + ret))
        return -EINVAL;
    if (nsda->header.length != (cpu_to_le16(sizeof(*nsda) - sizeof(struct nan_attribute_header) + nsda->service_info_length)))
        return -EINVAL;
    len += sizeof(*si) + ret;

    /* NAN Attribute for Service Descriptor extension header */
    if (len + sizeof(*nsdea) > buf_size)
        return -ENOMEM;
    nsdea = (struct nan_service_descriptor_extension_attribute *)(buf + len);
    if (nsdea->header.attribute_id != 0xE)
        return -EINVAL;
    if (nsdea->header.length != cpu_to_le16(0x0004))
        return -EINVAL;
    if (nsdea->instance_id != 0x01)
        return -EINVAL;
    if (nsdea->control != cpu_to_le16(0x0200))
        return -EINVAL;

    return 0;
}
