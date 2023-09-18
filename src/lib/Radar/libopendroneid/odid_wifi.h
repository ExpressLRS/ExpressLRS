/*
Copyright (C) 2019 Intel Corporation

SPDX-License-Identifier: Apache-2.0

Open Drone ID C Library

Maintainer:
Gabriel Cox
gabriel.c.cox@intel.com
*/

#ifndef _ODID_WIFI_H_
#define _ODID_WIFI_H_

/**
* IEEE 802.11 structs to build management action frame
*/
struct __attribute__((__packed__)) ieee80211_mgmt {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bssid[6];
    uint16_t seq_ctrl;
};

struct __attribute__((__packed__)) ieee80211_beacon {
    uint64_t timestamp;
    uint16_t beacon_interval;
    uint16_t capability;
};

struct __attribute__((__packed__)) ieee80211_ssid {
    uint8_t element_id;
    uint8_t length;
    uint8_t ssid[];
};

struct __attribute__((__packed__)) ieee80211_supported_rates {
    uint8_t element_id;
    uint8_t length;
    uint8_t supported_rates;
};

struct __attribute__((__packed__)) ieee80211_vendor_specific {
	uint8_t element_id;
	uint8_t length;
	uint8_t oui[3];
	uint8_t oui_type;
};

struct __attribute__((__packed__)) nan_service_discovery {
    uint8_t category;
    uint8_t action_code;
    uint8_t oui[3];
    uint8_t oui_type;
};

struct __attribute__((__packed__)) nan_attribute_header {
    uint8_t attribute_id;
    uint16_t length;
};

struct __attribute__((__packed__)) nan_master_indication_attribute {
    struct nan_attribute_header header;
    uint8_t master_preference;
    uint8_t random_factor;
};

struct __attribute__((__packed__)) nan_cluster_attribute {
    struct nan_attribute_header header;
    uint8_t device_mac[6];
    uint8_t random_factor;
    uint8_t master_preference;
    uint8_t hop_count_to_anchor_master;
    uint8_t anchor_master_beacon_transmission_time[4];
};

struct __attribute__((__packed__)) nan_service_id_list_attribute {
    struct nan_attribute_header header;
    uint8_t service_id[6];
};

struct __attribute__((__packed__)) nan_service_descriptor_attribute {
    struct nan_attribute_header header;
    uint8_t service_id[6];
    uint8_t instance_id;
    uint8_t requestor_instance_id;
    uint8_t service_control;
    uint8_t service_info_length;
};

struct __attribute__((__packed__)) nan_service_descriptor_extension_attribute {
    struct nan_attribute_header header;
    uint8_t instance_id;
    uint16_t control;
    uint8_t service_update_indicator;
};

struct __attribute__((__packed__)) ODID_service_info {
    uint8_t message_counter;
    ODID_MessagePack_encoded odid_message_pack[];
};

#endif // _ODID_WIFI_H_
