#include "stm32Updater.h"
#include <WebSocketsServer.h>

//adapted from https://github.com/mengguang/esp8266_stm32_isp

extern WebSocketsServer webSocket;

char log_buffer[256];

uint8_t memory_buffer[BLOCK_SIZE];
uint8_t file_buffer[BLOCK_SIZE];

#define DEBUG_PRINT(...) \
	do { \
		snprintf(log_buffer, sizeof(log_buffer), __VA_ARGS__); \
  		debug_log(); \
	}while(0);

void reset_stm32_to_isp_mode()
{
	// reset STM32 into DFU mode
	pinMode(RESET_PIN, OUTPUT);
	pinMode(BOOT0_PIN, OUTPUT);
	digitalWrite(BOOT0_PIN, HIGH);
	delay(100);
	digitalWrite(RESET_PIN, LOW);
	delay(50);
	digitalWrite(RESET_PIN, HIGH);
	delay(500);
	digitalWrite(BOOT0_PIN, LOW);
	pinMode(BOOT0_PIN, INPUT);
	pinMode(RESET_PIN, INPUT);
}

void reset_stm32_to_app_mode()
{
	pinMode(RESET_PIN, OUTPUT);
	pinMode(BOOT0_PIN, OUTPUT);
	digitalWrite(BOOT0_PIN, LOW);
	delay(100);
	digitalWrite(RESET_PIN, LOW);
	delay(50);
	digitalWrite(RESET_PIN, HIGH);
	pinMode(BOOT0_PIN, INPUT);
	pinMode(RESET_PIN, INPUT);
}

void stm32flasher_hardware_init()
{
	webSocket.broadcastTXT("STM32 Flasher Pin IO init");
	pinMode(RESET_PIN, INPUT);
	pinMode(BOOT0_PIN, INPUT);
	Serial.begin(115200, SERIAL_8E1);
	Serial.setTimeout(5000);
}

void debug_log()
{
	webSocket.broadcastTXT(log_buffer);
}

uint8_t isp_serial_write(uint8_t *buffer, uint8_t length)
{
	return Serial.write(buffer, length);
}

uint8_t isp_serial_read(uint8_t *buffer, uint8_t length)
{
	uint8_t timeout = 100;
	// wait until date is available
	while(!Serial.available() && timeout) {
		delay(1);
		timeout--;
	}
	if (!timeout)
		return 0;
	return Serial.readBytes(buffer, length);
}

uint8_t isp_serial_flush()
{
	while (Serial.available() > 0)
	{
		Serial.read();
	}
	return 1;
}

uint8_t wait_for_ack(char const *when)
{
	uint8_t timeout = 20;
	uint8_t cmd = 0;
	while (timeout--) {
		uint8_t nread = isp_serial_read(&cmd, 1);
		if (cmd == 0x79)
		{ // ack
			return 1;
		}
		else if (cmd == 0x1F)
		{ // nack
			DEBUG_PRINT("got NACK when: %s", when);
			return 2;
		}
		else if (cmd == 0x76)
		{ // busy
			DEBUG_PRINT("got BUSY when: %s", when);
			continue;
		}
		else if (nread)
		{
			DEBUG_PRINT("[WARNING] got unknown response: %d when: %s.", cmd, when);
			break;
		}
		else
		{
			delay(1);
		}
	}

	if (!timeout)
	{
		DEBUG_PRINT("[ERROR] no response when: %s.", when);
	}

	return 0;
}

uint8_t init_chip()
{
	uint8_t cmd = 0x7F;

	DEBUG_PRINT("trying to init chip...");

	for (int i = 0; i < 10; i++)
	{
		isp_serial_write(&cmd, 1);
		if (wait_for_ack("init_chip_write_cmd") > 0)
		{ // ack or nack
			DEBUG_PRINT("init chip succeeded.");
			return 1;
		}
	}
	DEBUG_PRINT("[ERROR] init chip failed.");
	return 0;
}

uint8_t cmd_generic(uint8_t command)
{
	isp_serial_flush();
	uint8_t cmd[2];
	cmd[0] = command;
	cmd[1] = command ^ 0xFF;
	isp_serial_write(cmd, 2);
	return wait_for_ack("cmd_generic");
}

uint8_t cmd_get()
{
	uint8_t bootloader_ver = 0;
	if (cmd_generic(0x00))
	{
		uint8_t * buffer = (uint8_t*)malloc(257);
		if (!buffer) return 0;
		isp_serial_read(buffer, 2);
		uint8_t len = buffer[0];
		bootloader_ver = buffer[1];
		isp_serial_read(buffer, len);
		free(buffer);
		if (wait_for_ack("cmd_get") != 1)
			return 0;

		DEBUG_PRINT("Bootloader version: 0x%02X", bootloader_ver);

		if (bootloader_ver < 20 || bootloader_ver >= 100) {
			DEBUG_PRINT("[ERROR] Invalid bootloader");
			bootloader_ver = 0;
		}
	}
	return bootloader_ver;
}

uint8_t cmd_getID()
{
	uint8_t retval = 0;
	if (cmd_generic(0x02))
	{
		uint8_t buffer[3] = {0, 0, 0};
		isp_serial_read(buffer, 3);

		uint16_t id = buffer[1];
		id <<= 8;
		id += buffer[2];

		DEBUG_PRINT("Chip ID: 0x%X", id);

		retval = wait_for_ack("cmd_getID");

		if (id != 0x410) {
			DEBUG_PRINT("[ERROR] Wrong ID. No R9M found!");
			retval = 0;
		}
	}
	return retval;
}

void encode_address(uint32_t address, uint8_t *result)
{
	uint8_t b3 = (uint8_t)((address >> 0) & 0xFF);
	uint8_t b2 = (uint8_t)((address >> 8) & 0xFF);
	uint8_t b1 = (uint8_t)((address >> 16) & 0xFF);
	uint8_t b0 = (uint8_t)((address >> 24) & 0xFF);

	uint8_t crc = (uint8_t)(b0 ^ b1 ^ b2 ^ b3);
	result[0] = b0;
	result[1] = b1;
	result[2] = b2;
	result[3] = b3;
	result[4] = crc;
}

uint8_t cmd_read_memory(uint32_t address, uint8_t length)
{
	if (cmd_generic(0x11) == 1)
	{
		uint8_t enc_address[5] = {0};
		encode_address(address, enc_address);
		isp_serial_write(enc_address, 5);
		if (wait_for_ack("write_address") != 1)
		{
			return 0;
		}
		uint8_t nread = length - 1;
		uint8_t crc = nread ^ 0xFF;
		uint8_t buffer[2];
		buffer[0] = nread;
		buffer[1] = crc;
		isp_serial_write(buffer, 2);
		if (wait_for_ack("write_address") != 1)
		{
			return 0;
		}
		uint8_t nreaded = 0;
		while (nreaded < length)
		{
			nreaded += isp_serial_read(memory_buffer + nreaded, length - nreaded);
		}
		return 1;
	}
	return 0;
}

uint8_t cmd_write_memory(uint32_t address, uint8_t length)
{
	if (cmd_generic(0x31) == 1)
	{
		uint8_t enc_address[5] = {0};
		encode_address(address, enc_address);
		isp_serial_write(enc_address, 5);
		if (wait_for_ack("write_address") != 1)
		{
			return 0;
		}
		uint8_t nwrite = length - 1;
		uint8_t buffer[1];
		buffer[0] = nwrite;
		isp_serial_write(buffer, 1);
		uint8_t crc = nwrite;
		for (int i = 0; i < length; i++)
		{
			crc = crc ^ memory_buffer[i];
		}
		isp_serial_write(memory_buffer, length);
		buffer[0] = crc;
		isp_serial_write(buffer, 1);
		return wait_for_ack("write_data");
	}
	return 0;
}

// if bootversion < 0x30
// pg 7 of https://www.st.com/resource/en/application_note/cd00264342-usart-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf
static uint8_t cmd_erase_all_memory(uint32_t filesize)
{
	if (cmd_generic(0x43) == 1)
	{
#if (FLASH_OFFSET == 0)
		DEBUG_PRINT("erasing all memory...");
		// Global erase aka mass erase
		uint8_t cmd[2];
		cmd[0] = 0xFF;
		cmd[1] = 0x00;
		isp_serial_write(cmd, sizeof(cmd));
		return wait_for_ack("mass_erase");
#else
		// Erase only defined pages
		uint8_t checksum = 0, page;
		uint8_t pages = ((FLASH_SIZE - FLASH_OFFSET) / FLASH_PAGE_SIZE);
		if (filesize)
			pages = (filesize + (FLASH_PAGE_SIZE - 1)) / FLASH_PAGE_SIZE;
		uint8_t page_offset = (FLASH_OFFSET / FLASH_PAGE_SIZE);
		DEBUG_PRINT("erasing pages: %u...%u", page_offset, (page_offset + pages));
		// Send to DFU
		Serial.write((uint8_t)(pages-1));
		checksum ^= (pages-1);
		for (page = page_offset; page < (page_offset + pages); page++) {
			Serial.write(page);
			checksum ^= page;
		}
		Serial.write(checksum);
		return wait_for_ack("erase_pages");
#endif
	}
	return 0;
}

// else if bootversion >= 0x30
static uint8_t cmd_erase_all_memory_extended()
{
	if (cmd_generic(0x44) == 1)
	{
#if (FLASH_OFFSET == 0)
		uint8_t cmd[3];
		cmd[0] = 0xFF;
		cmd[1] = 0xFF;
		cmd[2] = 0x00;
		isp_serial_write(cmd, 3);
		return wait_for_ack("mass_erase");
#else
		DEBUG_PRINT("[ERROR] extended erase implementation missing!");
#endif
	}
	return 0;
}

uint8_t cmd_erase(uint32_t filesize, uint8_t bootloader_ver)
{
	if (bootloader_ver < 0x30)
		return cmd_erase_all_memory(filesize);
	return cmd_erase_all_memory_extended();
}

uint8_t cmd_go(uint32_t address)
{
	if (cmd_generic(0x21) == 1)
	{
		uint8_t enc_address[5] = {0};
		encode_address(address, enc_address);
		isp_serial_write(enc_address, 5);
		return wait_for_ack("cmd_go");
	}
	return 0;
}

uint8_t esp8266_spifs_write_file(const char *filename)
{
	if (!SPIFFS.exists(filename))
	{
		DEBUG_PRINT("[ERROR] file: %s doesn't exist!", filename);
		return 0;
	}
	File fp = SPIFFS.open(filename, "r");
	uint32_t filesize = fp.size();
	DEBUG_PRINT("filesize: %d", filesize);
	if ((FLASH_SIZE - FLASH_OFFSET) < filesize) {
		DEBUG_PRINT("[ERROR] file is too big!");
		fp.close();
		return 0;
	}

	reset_stm32_to_isp_mode();
	isp_serial_flush();

	if (init_chip() != 1) {
		fp.close();
		return 0;
	}

	uint8_t bootloader_ver = cmd_get();
	if (bootloader_ver == 0) {
		fp.close();
		return 0;
	}

	if (cmd_getID() != 1) {
		fp.close();
		return 0;
	}

	if (cmd_erase(filesize, bootloader_ver) != 1)
	{
		DEBUG_PRINT("[ERROR] erase Failed!");
		fp.close();
		return 0;
	}
	DEBUG_PRINT("erase Success!");

	DEBUG_PRINT("begin to write file.");
	uint32_t junks = (filesize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
	uint32_t junk = 0;
	uint8_t proc;
	while (fp.position() < filesize)
	{
		uint8_t nread = BLOCK_SIZE;
		if ((filesize - fp.position()) < BLOCK_SIZE)
		{
			nread = filesize - fp.position();
		}
		nread = fp.readBytes((char *)memory_buffer, nread);

		//DEBUG_PRINT("write bytes: %d, file position: %d", nread, fp.position());
		proc = (++junk * 100) / junks;
		if ((junk % 40) == 0 || junk >= junks)
			DEBUG_PRINT("Write %u%%", proc);

		uint8_t result = cmd_write_memory(BEGIN_ADDRESS + fp.position() - nread, nread);
		if (result != 1)
		{
			DEBUG_PRINT("[ERROR] file write failed.");
			fp.close();
			return 0;
		}
	}
	DEBUG_PRINT("file write succeeded.");
	DEBUG_PRINT("begin to verify file.");
	fp.seek(0, SeekSet);
	junk = 0;
	while (fp.position() < filesize)
	{
		uint8_t nread = BLOCK_SIZE;
		if ((filesize - fp.position()) < BLOCK_SIZE)
		{
			nread = filesize - fp.position();
		}
		nread = fp.readBytes((char *)file_buffer, nread);

		//DEBUG_PRINT("read bytes: %d, file position: %d", nread, fp.position());
		proc = (++junk * 100) / junks;
		if ((junk % 40) == 0 || junk >= junks)
			DEBUG_PRINT("Verify %u%%", proc);

		uint8_t result = cmd_read_memory(BEGIN_ADDRESS + fp.position() - nread, nread);
		if (result != 1)
		{
			DEBUG_PRINT("[ERROR] read memory failed: %d", fp.position());
			fp.close();
			return 0;
		}
		result = memcmp(file_buffer, memory_buffer, nread);
		if (result != 0)
		{
			DEBUG_PRINT("[ERROR] verify failed.");
			fp.close();
			return 0;
		}
	}

	fp.close();
	DEBUG_PRINT("verify file succeeded.");
	DEBUG_PRINT("start application.");
	//reset_stm32_to_app_mode();
	cmd_go(FLASH_START);
	return 1;
}
