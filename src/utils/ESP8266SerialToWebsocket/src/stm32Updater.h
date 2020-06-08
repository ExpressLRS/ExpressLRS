#pragma once
#include "FS.h"

// taken and adapted from https://github.com/mengguang/esp8266_stm32_isp

#define BLOCK_SIZE 128
#ifndef BOOT0_PIN
#define BOOT0_PIN 4
#endif
#ifndef RESET_PIN
#define RESET_PIN 5
#endif

#define FLASH_START 0x08000000
#define FLASH_SIZE 0x10000
#define FLASH_PAGE_SIZE 0x400
#define FLASH_OFFSET 0x2000 // skip bootloader
#define BEGIN_ADDRESS (FLASH_START + FLASH_OFFSET)

uint8_t start_key_pressed();

void reset_stm32_to_isp_mode();

void reset_stm32_to_app_mode();

void stm32flasher_hardware_init();

void debug_log();

uint8_t isp_serial_write(uint8_t *buffer, uint8_t length);

uint8_t isp_serial_read(uint8_t *buffer, uint8_t length);

uint8_t isp_serial_flush();

uint8_t wait_for_ack(char *when);

uint8_t init_chip();

uint8_t cmd_generic(uint8_t command);

uint8_t cmd_get();

void encode_address(uint32_t address, uint8_t *result);

uint8_t cmd_read_memory(uint32_t address, uint8_t length);

uint8_t cmd_write_memory(uint32_t address, uint8_t length);

uint8_t cmd_erase(uint32_t filesize, uint8_t bootloader_ver);

uint8_t cmd_go(uint32_t address);

uint8_t esp8266_spifs_write_file(const char *filename);