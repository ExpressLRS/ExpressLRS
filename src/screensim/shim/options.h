#pragma once
#include "targets.h"
extern const char *wifi_hostname, *wifi_ap_ssid, *wifi_ap_password, *wifi_ap_address;
extern uint32_t logo_image;
#define ESP_OK 0
int spi_flash_read(uint32_t addr, void *dst, size_t n);
