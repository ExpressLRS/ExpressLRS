#if defined(TARGET_RX) && defined(PLATFORM_ESP32)
//
// Name:		DShotRMT.cpp
// Created: 	20.03.2021 00:49:15
// Author:  	derdoktor667
//

#include "DShotRMT.h"

DShotRMT::DShotRMT(gpio_num_t gpio, rmt_channel_t rmtChannel) : gpio_num(gpio), rmt_channel(rmtChannel) {
	// ...create clean packet
	encode_dshot_to_rmt(DSHOT_NULL_PACKET);
}

DShotRMT::~DShotRMT() {
	rmt_tx_stop(rmt_channel);
	rmt_driver_uninstall(rmt_channel);
}

bool DShotRMT::begin(dshot_mode_t dshot_mode, bool is_bidirectional) {
	mode = dshot_mode;
	bidirectional = is_bidirectional;

	uint16_t ticks_per_bit;

	switch (mode) {
		case DSHOT150:
			ticks_per_bit = 64; // ...Bit Period Time 6.67 us
			ticks_zero_high = 24; // ...zero time 2.50 us
			ticks_one_high = 48; // ...one time 5.00 us
			break;

		case DSHOT300:
			ticks_per_bit = 32; // ...Bit Period Time 3.33 us
			ticks_zero_high = 12; // ...zero time 1.25 us
			ticks_one_high = 24; // ...one time 2.50 us
			break;

		case DSHOT600:
			ticks_per_bit = 16; // ...Bit Period Time 1.67 us
			ticks_zero_high = 6; // ...zero time 0.625 us
			ticks_one_high = 12; // ...one time 1.25 us
			break;

		case DSHOT1200:
			ticks_per_bit = 8; // ...Bit Period Time 0.83 us
			ticks_zero_high = 3; // ...zero time 0.313 us
			ticks_one_high = 6; // ...one time 0.625 us
			break;

		// ...because having a default is "good style"
		default:
			ticks_per_bit = 0; // ...Bit Period Time endless
			ticks_zero_high = 0; // ...no bits, no time
			ticks_one_high = 0; // ......no bits, no time
			break;
	}

	// ...calc low signal timing
	ticks_zero_low = (ticks_per_bit - ticks_zero_high);
	ticks_one_low = (ticks_per_bit - ticks_one_high);

	rmt_config_t dshot_tx_rmt_config = {
		.rmt_mode = RMT_MODE_TX,
		.channel = rmt_channel,
		.gpio_num = gpio_num,
		.clk_div = DSHOT_CLK_DIVIDER,
		.mem_block_num = uint8_t(RMT_CHANNEL_MAX - uint8_t(rmt_channel)),
		.tx_config = {
        	.idle_level = bidirectional ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW,
			.carrier_en = false,
			.loop_en = true,
			.idle_output_en = true,
		},
	};

	// ...pause "bit" added to each frame
    if (bidirectional) {
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = HIGH;
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = HIGH;
    } else {
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = LOW;
        dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = LOW;
    }

	dshot_tx_rmt_item[DSHOT_PAUSE_BIT].duration1 = 0;
	dshot_tx_rmt_item[DSHOT_PAUSE_BIT].duration0 = 10000 - (16*ticks_per_bit) - 1;

	// setup the RMT end marker
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].duration0 = 0;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].level0 = HIGH;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].duration1 = 0;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].level1 = LOW;

	// ...setup selected dshot mode
	rmt_config(&dshot_tx_rmt_config);

	// ...essential step, return the result
	return rmt_driver_install(dshot_tx_rmt_config.channel, 0, 0);
}

// ...the config part is done, now the calculating and sending part
void DShotRMT::send_dshot_value(uint16_t throttle_value, telemetric_request_t telemetric_request) {
	dshot_packet_t dshot_rmt_packet = { };
	
	if (throttle_value == 0) {
		dshot_rmt_packet.throttle_value = 0;
	} else {
		if (throttle_value < DSHOT_THROTTLE_MIN) {
			throttle_value = DSHOT_THROTTLE_MIN;
		}

		if (throttle_value > DSHOT_THROTTLE_MAX) {
			throttle_value = DSHOT_THROTTLE_MAX;
		}

		// ...packets are the same for bidirectional mode
		dshot_rmt_packet.throttle_value = throttle_value;
	}
	
	dshot_rmt_packet.telemetric_request = telemetric_request;
	dshot_rmt_packet.checksum = this->calc_dshot_chksum(dshot_rmt_packet);

	output_rmt_data(dshot_rmt_packet);
}

rmt_item32_t* DShotRMT::encode_dshot_to_rmt(uint16_t parsed_packet) {
    // ...is bidirecional mode activated
    if (bidirectional) {
        // ..."invert" the signal duration
        for (int i = 0; i < DSHOT_PAUSE_BIT; i++, parsed_packet <<= 1) 	{
		    if (parsed_packet & 0b1000000000000000) {
			    // set one
			    dshot_tx_rmt_item[i].duration0 = ticks_one_low;
			    dshot_tx_rmt_item[i].duration1 = ticks_one_high;
		    }
		    else {
			    // set zero
			    dshot_tx_rmt_item[i].duration0 = ticks_zero_low;
			    dshot_tx_rmt_item[i].duration1 = ticks_zero_high;
		    }

			dshot_tx_rmt_item[i].level0 = LOW;
			dshot_tx_rmt_item[i].level1 = HIGH;
        }
    }

    // ..."normal" DShot mode / "bidirectional" mode OFF
    else {
        for (int i = 0; i < DSHOT_PAUSE_BIT; i++, parsed_packet <<= 1) 	{
		    if (parsed_packet & 0b1000000000000000) {
			    // set one
			    dshot_tx_rmt_item[i].duration0 = ticks_one_high;
			    dshot_tx_rmt_item[i].duration1 = ticks_one_low;
		    }
		    else {
			    // set zero
			    dshot_tx_rmt_item[i].duration0 = ticks_zero_high;
			    dshot_tx_rmt_item[i].duration1 = ticks_zero_low;
		    }

			dshot_tx_rmt_item[i].level0 = HIGH;
			dshot_tx_rmt_item[i].level1 = LOW;
        }
    }

	return dshot_tx_rmt_item;
}

// ...just returns the checksum
// DOES NOT APPEND CHECKSUM!!!
uint16_t DShotRMT::calc_dshot_chksum(const dshot_packet_t& dshot_packet) {
	// ...same initial 12bit data for bidirectional or "normal" mode
	uint16_t packet = (dshot_packet.throttle_value << 1) | dshot_packet.telemetric_request;

	if (bidirectional) {
		// ...calc the checksum "inverted" / bidirectional mode
		return (~(packet ^ (packet >> 4) ^ (packet >> 8))) & 0x0F;
	} else {
		// ...calc the checksum "normal" mode
		return (packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0F;
	}
}

uint16_t DShotRMT::prepare_rmt_data(const dshot_packet_t& dshot_packet) {
	auto chksum = calc_dshot_chksum(dshot_packet);

    // ..."construct" the packet
	uint16_t prepared_to_encode = (dshot_packet.throttle_value << 1) | dshot_packet.telemetric_request;
	prepared_to_encode = (prepared_to_encode << 4) | chksum;

	return prepared_to_encode;
}

// ...finally output using ESP32 RMT
void DShotRMT::output_rmt_data(const dshot_packet_t& dshot_packet) {
	encode_dshot_to_rmt(prepare_rmt_data(dshot_packet));

	rmt_tx_stop(rmt_channel);
	rmt_fill_tx_items(rmt_channel, dshot_tx_rmt_item, DSHOT_PACKET_LENGTH, 0);
	rmt_tx_start(rmt_channel, true);
}
#endif