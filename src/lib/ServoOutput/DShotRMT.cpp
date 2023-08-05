#if defined(PLATFORM_ESP32)
//
// Name:		DShotRMT.cpp
// Created: 	20.03.2021 00:49:15
// Author:  	derdoktor667
//

#include "DShotRMT.h"

DShotRMT::DShotRMT(gpio_num_t gpio, rmt_channel_t rmtChannel) {
    dshot_config.gpio_num = gpio;
	dshot_config.pin_num = uint8_t(gpio);
	dshot_config.rmt_channel = rmtChannel;
	dshot_config.mem_block_num = uint8_t(RMT_CHANNEL_MAX - uint8_t(rmtChannel));

	// ...create clean packet
	encode_dshot_to_rmt(DSHOT_NULL_PACKET);
}

DShotRMT::DShotRMT(uint8_t pin, uint8_t channel) {
	dshot_config.gpio_num = gpio_num_t(pin);
	dshot_config.pin_num = pin;
	dshot_config.rmt_channel = rmt_channel_t(channel);
	dshot_config.mem_block_num = (RMT_CHANNEL_MAX - channel);

	// ...create clean packet
	encode_dshot_to_rmt(DSHOT_NULL_PACKET);
}

DShotRMT::~DShotRMT() {
	rmt_driver_uninstall(dshot_config.rmt_channel);
}

DShotRMT::DShotRMT(DShotRMT const&) {
	// ...write me	
}

bool DShotRMT::begin(dshot_mode_t dshot_mode, bool is_bidirectional) {
	dshot_config.mode = dshot_mode;
	dshot_config.clk_div = DSHOT_CLK_DIVIDER;
	dshot_config.name_str = dshot_mode_name[dshot_mode];
	dshot_config.bidirectional = is_bidirectional;

	switch (dshot_config.mode) {
		case DSHOT150:
			dshot_config.ticks_per_bit = 64; // ...Bit Period Time 6.67 us
			dshot_config.ticks_zero_high = 24; // ...zero time 2.50 us
			dshot_config.ticks_one_high = 48; // ...one time 5.00 us
			break;

		case DSHOT300:
			dshot_config.ticks_per_bit = 32; // ...Bit Period Time 3.33 us
			dshot_config.ticks_zero_high = 12; // ...zero time 1.25 us
			dshot_config.ticks_one_high = 24; // ...one time 2.50 us
			break;

		case DSHOT600:
			dshot_config.ticks_per_bit = 16; // ...Bit Period Time 1.67 us
			dshot_config.ticks_zero_high = 6; // ...zero time 0.625 us
			dshot_config.ticks_one_high = 12; // ...one time 1.25 us
			break;

		case DSHOT1200:
			dshot_config.ticks_per_bit = 8; // ...Bit Period Time 0.83 us
			dshot_config.ticks_zero_high = 3; // ...zero time 0.313 us
			dshot_config.ticks_one_high = 6; // ...one time 0.625 us
			break;

		// ...because having a default is "good style"
		default:
			dshot_config.ticks_per_bit = 0; // ...Bit Period Time endless
			dshot_config.ticks_zero_high = 0; // ...no bits, no time
			dshot_config.ticks_one_high = 0; // ......no bits, no time
			break;
	}

	// ...calc low signal timing
	dshot_config.ticks_zero_low = (dshot_config.ticks_per_bit - dshot_config.ticks_zero_high);
	dshot_config.ticks_one_low = (dshot_config.ticks_per_bit - dshot_config.ticks_one_high);

	dshot_tx_rmt_config.rmt_mode = RMT_MODE_TX;
	dshot_tx_rmt_config.channel = dshot_config.rmt_channel;
	dshot_tx_rmt_config.gpio_num = dshot_config.gpio_num;
	dshot_tx_rmt_config.mem_block_num = dshot_config.mem_block_num;
	dshot_tx_rmt_config.clk_div = dshot_config.clk_div;

	dshot_tx_rmt_config.tx_config.loop_en = false;
	dshot_tx_rmt_config.tx_config.carrier_en = false;
	dshot_tx_rmt_config.tx_config.idle_output_en = true;

    if (dshot_config.bidirectional) {
        dshot_tx_rmt_config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
    } else {
        dshot_tx_rmt_config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    }

	// ...setup selected dshot mode
	rmt_config(&dshot_tx_rmt_config);

	// ...essential step, return the result
	return rmt_driver_install(dshot_tx_rmt_config.channel, 0, 0);
}

// ...the config part is done, now the calculating and sending part
void DShotRMT::send_dshot_value(uint16_t throttle_value, telemetric_request_t telemetric_request) {
	dshot_packet_t dshot_rmt_packet = { };

	if (throttle_value < DSHOT_THROTTLE_MIN) {
		throttle_value = DSHOT_THROTTLE_MIN; 
	}

	if (throttle_value > DSHOT_THROTTLE_MAX) {
		throttle_value = DSHOT_THROTTLE_MAX;
	}

	// ...packets are the same for bidirectional mode
	dshot_rmt_packet.throttle_value = throttle_value;
	dshot_rmt_packet.telemetric_request = telemetric_request;
	dshot_rmt_packet.checksum = this->calc_dshot_chksum(dshot_rmt_packet);

	output_rmt_data(dshot_rmt_packet);
}

// ...get all setup data about current mode
dshot_config_t* DShotRMT::get_dshot_info() {
	return &dshot_config;
}

// ...get the ABP_Clock Divider for further calculations
uint8_t* DShotRMT::get_dshot_clock_div() {
	return &dshot_config.clk_div;
}

rmt_item32_t* DShotRMT::encode_dshot_to_rmt(uint16_t parsed_packet) {
    // ...is bidirecional mode activated
    if (dshot_config.bidirectional) {
        // ..."invert" the signal duration
        for (int i = 0; i < DSHOT_PAUSE_BIT; i++, parsed_packet <<= 1) 	{
		    if (parsed_packet & 0b1000000000000000) {
			    // set one
			    dshot_tx_rmt_item[i].duration0 = dshot_config.ticks_one_low;
			    dshot_tx_rmt_item[i].duration1 = dshot_config.ticks_one_high;
		    }
		    else {
			    // set zero
			    dshot_tx_rmt_item[i].duration0 = dshot_config.ticks_zero_low;
			    dshot_tx_rmt_item[i].duration1 = dshot_config.ticks_zero_high;
		    }

		dshot_tx_rmt_item[i].level0 = 0;
		dshot_tx_rmt_item[i].level1 = 1;
        }
    }
    
    // ..."normal" DShot mode / "bidirectional" mode OFF
    else {
        for (int i = 0; i < DSHOT_PAUSE_BIT; i++, parsed_packet <<= 1) 	{
		    if (parsed_packet & 0b1000000000000000) {
			    // set one
			    dshot_tx_rmt_item[i].duration0 = dshot_config.ticks_one_high;
			    dshot_tx_rmt_item[i].duration1 = dshot_config.ticks_one_low;
		    }
		    else {
			    // set zero
			    dshot_tx_rmt_item[i].duration0 = dshot_config.ticks_zero_high;
			    dshot_tx_rmt_item[i].duration1 = dshot_config.ticks_zero_low;
		    }

		dshot_tx_rmt_item[i].level0 = 1;
		dshot_tx_rmt_item[i].level1 = 0;
        }
    }

    if (dshot_config.bidirectional) {
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = 1;
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = 0;
    } else {
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = 0;
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = 1;
    }

	// ...end marker added to each frame
	dshot_tx_rmt_item[DSHOT_PAUSE_BIT].duration1 = (DSHOT_PAUSE / 2);

	return dshot_tx_rmt_item;
}

// ...just returns the checksum
// DOES NOT APPEND CHECKSUM!!!
uint16_t DShotRMT::calc_dshot_chksum(const dshot_packet_t& dshot_packet) {
    // ...start with two emprty "container"
	uint16_t packet = DSHOT_NULL_PACKET;
	uint16_t chksum = DSHOT_NULL_PACKET;

	// ...same initial 12bit data for bidirectional or "normal" mode
	packet = (dshot_packet.throttle_value << 1) | dshot_packet.telemetric_request;

	if (dshot_config.bidirectional) {
		// ...calc the checksum "inverted" / bidirectional mode
		chksum = (~(packet ^ (packet >> 4) ^ (packet >> 8))) & 0x0F;
	} else {
		// ...calc the checksum "normal" mode
		chksum = (packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0F;
	}

	return chksum;
}

uint16_t DShotRMT::prepare_rmt_data(const dshot_packet_t& dshot_packet) {
	uint16_t prepared_to_encode = DSHOT_NULL_PACKET;
	uint16_t chksum = calc_dshot_chksum(dshot_packet);

    // ..."construct" the packet
	prepared_to_encode = (dshot_packet.throttle_value << 1) | dshot_packet.telemetric_request;
	prepared_to_encode = (prepared_to_encode << 4) | chksum;

	return prepared_to_encode;
}

// ...finally output using ESP32 RMT
void DShotRMT::output_rmt_data(const dshot_packet_t& dshot_packet) {
	encode_dshot_to_rmt(prepare_rmt_data(dshot_packet));

	//
	rmt_write_items(dshot_tx_rmt_config.channel, dshot_tx_rmt_item, DSHOT_PACKET_LENGTH, false);
}
#endif