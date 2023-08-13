#if (defined(GPIO_PIN_PWM_OUTPUTS) && defined(PLATFORM_ESP32))
//
// Name:		DShotRMT.h
// Created: 	20.03.2021 00:49:15
// Author:  	derdoktor667
//

#pragma once

#include <Arduino.h>
// #include "BlheliCmdMap.h"
// Added back to DShotRMT.h

// ...utilizing the IR Module library for generating the DShot signal
#include <driver/rmt.h>

constexpr auto DSHOT_CLK_DIVIDER = 8; // ...slow down RMT clock to 0.1 microseconds / 100 nanoseconds per cycle
constexpr auto DSHOT_PACKET_LENGTH = 18; // ...last packet is the pause followed by RMT end marker

constexpr auto DSHOT_THROTTLE_MIN = 48;
constexpr auto DSHOT_THROTTLE_MAX = 2047;
constexpr auto DSHOT_NULL_PACKET = 0b0000000000000000;

constexpr auto DSHOT_PAUSE = 21; // ...21bit is recommended, but to be sure
constexpr auto DSHOT_PAUSE_BIT = 16;

constexpr auto F_CPU_RMT = APB_CLK_FREQ;
constexpr auto RMT_CYCLES_PER_SEC = (F_CPU_RMT / DSHOT_CLK_DIVIDER);
constexpr auto RMT_CYCLES_PER_ESP_CYCLE = (F_CPU / RMT_CYCLES_PER_SEC);

// Source:	https://github.com/bitdump/BLHeli/blob/master/BLHeli_S%20SiLabs/Dshotprog%20spec%20BLHeli_S.txt
// Date:	04.07.2021

enum dshot_cmd_t {
	DSHOT_CMD_MOTOR_STOP,				// Currently not implemented - STOP Motors
	DSHOT_CMD_BEEP1,					// Wait at least length of beep (380ms) before next command
	DSHOT_CMD_BEEP2,					// Wait at least length of beep (380ms) before next command
	DSHOT_CMD_BEEP3,					// Wait at least length of beep (400ms) before next command
	DSHOT_CMD_BEEP4,					// Wait at least length of beep (400ms) before next command
	DSHOT_CMD_BEEP5,					// Wait at least length of beep (400ms) before next command
	DSHOT_CMD_ESC_INFO, 				// Currently not implemented
	DSHOT_CMD_SPIN_DIRECTION_1,			// Need 6x, no wait required
	DSHOT_CMD_SPIN_DIRECTION_2,			// Need 6x, no wait required
	DSHOT_CMD_3D_MODE_OFF,				// Need 6x, no wait required
	DSHOT_CMD_3D_MODE_ON, 				// Need 6x, no wait required
	DSHOT_CMD_SETTINGS_REQUEST,			// Currently not implemented
	DSHOT_CMD_SAVE_SETTINGS,			// Need 6x, wait at least 12ms before next command
	DSHOT_CMD_SPIN_DIRECTION_NORMAL,	// Need 6x, no wait required
	DSHOT_CMD_SPIN_DIRECTION_REVERSED,	// Need 6x, no wait required
	DSHOT_CMD_LED0_ON,					// Currently not implemented
	DSHOT_CMD_LED1_ON,					// Currently not implemented
	DSHOT_CMD_LED2_ON,					// Currently not implemented
	DSHOT_CMD_LED3_ON,					// Currently not implemented
	DSHOT_CMD_LED0_OFF,					// Currently not implemented
	DSHOT_CMD_LED1_OFF,					// Currently not implemented
	DSHOT_CMD_LED2_OFF,					// Currently not implemented
	DSHOT_CMD_LED3_OFF,					// Currently not implemented
	DSHOT_CMD_MAX = 47
};

typedef enum dshot_mode_e {
	DSHOT_OFF,
	DSHOT150,
	DSHOT300,
	DSHOT600,
	DSHOT1200
} dshot_mode_t;

typedef enum telemetric_request_e {
	NO_TELEMETRIC,
	ENABLE_TELEMETRIC,
} telemetric_request_t;

// ...set bitcount for DShot packet
typedef struct dshot_packet_s {
	uint16_t throttle_value	: 11;
	telemetric_request_t telemetric_request : 1;
	uint16_t checksum : 4;
} dshot_packet_t;

// ...set bitcount for eRPM packet
typedef struct eRPM_packet_s {
    uint16_t eRPM_data : 12;
    uint8_t checksum : 4;
} eRPM_packet_t;

// ...all settings for the dshot mode
typedef struct dshot_config_s {
} dshot_config_t;

class DShotRMT {
public:
	DShotRMT(gpio_num_t gpio, rmt_channel_t rmtChannel);
	~DShotRMT();

	// ...safety first ...no parameters, no DShot
	bool begin(dshot_mode_t dshot_mode = DSHOT_OFF, bool is_bidirectional = false);
	void send_dshot_value(uint16_t throttle_value, telemetric_request_t telemetric_request = NO_TELEMETRIC);

private:
	gpio_num_t gpio_num;
	rmt_channel_t rmt_channel;
	rmt_item32_t dshot_tx_rmt_item[DSHOT_PACKET_LENGTH + 1];

	dshot_mode_t mode = DSHOT_OFF;
	bool bidirectional = false;
	uint16_t ticks_zero_high = 0;
	uint16_t ticks_zero_low = 0;
	uint16_t ticks_one_high = 0;
	uint16_t ticks_one_low = 0;

	rmt_item32_t* encode_dshot_to_rmt(uint16_t parsed_packet);
	uint16_t calc_dshot_chksum(const dshot_packet_t& dshot_packet);
	uint16_t prepare_rmt_data(const dshot_packet_t& dshot_packet);

	void output_rmt_data(const dshot_packet_t& dshot_packet);
};
#endif