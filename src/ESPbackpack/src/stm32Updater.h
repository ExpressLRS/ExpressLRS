#pragma once
#include "FS.h"

// taken and adapted from https://github.com/mengguang/esp8266_stm32_isp

#ifndef BOOT0_PIN
#define BOOT0_PIN 4
#endif
#ifndef RESET_PIN
#define RESET_PIN 5
#endif

#define FLASH_START 0x08000000
#define FLASH_SIZE 0x10000
#define FLASH_PAGE_SIZE 0x400
#define FLASH_OFFSET 0x4000
#define BEGIN_ADDRESS (FLASH_START + FLASH_OFFSET)


void reset_stm32_to_isp_mode();

void reset_stm32_to_app_mode();

void stm32flasher_hardware_init();

void debug_log();

uint8_t esp8266_spifs_write_file(const char *filename, uint32_t const begin_addr);
