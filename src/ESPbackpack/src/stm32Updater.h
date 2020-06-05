#pragma once
#include "FS.h"

//taken and adapted from https://github.com/mengguang/esp8266_stm32_isp

//SoftwareSerial debugSerial(13, 12, false, 256); //rx tx inverse buffer

//#define ispSerial Serial
#define BLOCK_SIZE 128
#define BOOT0_PIN 4
#define RESET_PIN 5
#define BEGIN_ADDRESS 0x08000000

uint8_t reset_stm32_to_isp_mode();

uint8_t reset_stm32_to_app_mode();

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

uint8_t cmd_erase_all_memory();

uint8_t cmd_go(uint32_t address);

uint8_t esp8266_spifs_write_file(char *filename);