#if defined(TARGET_RX) && defined(PLATFORM_ESP32)
//
// Name:		DShotRMT.cpp
// Created: 	20.03.2021 00:49:15
// Author:  	derdoktor667
//

#include "DShotRMT.h"
#include "common.h"
#include "config.h"
#include "logging.h"

static const rmt_channel_t rmt_channel = (rmt_channel_t)0; // instead of using many RMT channels, we are using just one, channel 0 is capable of transmitting on both ESP32-classic and ESP32-C3 so we pick 0

static DShotRMT *head_node = NULL, *cur_node = NULL, *tail_node; // linked list of all initailized RMT instances

static int prev_pin = -1; // we need to remember what the previous GPIO pin used was to remove it from the RMT (setting a new RMT pin does not unset the previous pin)

// latch status to indicate if the RMT driver has been installed or uninstalled, so we don't do it twice 
static bool has_inited = false;
static bool has_deinited = false;

static rmt_item32_t dshot_tx_rmt_item[DSHOT_PACKET_LENGTH + 1];

DShotRMT::DShotRMT(gpio_num_t gpio, int idx) : gpio_num(gpio), my_idx(idx) {
}

DShotRMT::~DShotRMT() {
	if (has_deinited) {
		// only allow uninstall once, not important but prevents errors over the debug log
		return;
	}
	rmt_tx_stop(rmt_channel);
	rmt_driver_uninstall(rmt_channel);
	has_deinited = true;
	head_node = NULL; // this isn't proper but all the instances are deleted at once anyways
	cur_node = NULL;
}

bool DShotRMT::begin(dshot_mode_t dshot_mode, bool is_bidirectional) {

	if (head_node == NULL) {
		// first one, initialize linked list
		head_node = this;
		tail_node = this;
		cur_node = this;
		this->next_node = (DShotRMT*)this;
	}
	else {
		// not the first node, so insert it as the next node
		tail_node->next_node = (DShotRMT*)this;
		tail_node = this;
		this->next_node = head_node;
	}

	mode = dshot_mode;
	bidirectional = is_bidirectional;

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
			.loop_en = false,
			.idle_output_en = true,
		},
	};

	// setup the RMT end marker
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].duration0 = 0;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].level0 = HIGH;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].duration1 = 0;
	dshot_tx_rmt_item[DSHOT_PACKET_LENGTH-1].level1 = LOW;

	if (has_inited) { // only do installation once
		return true;
	}

	// ...setup selected dshot mode
	rmt_config(&dshot_tx_rmt_config);

	// ...essential step, return the result
	esp_err_t ret = rmt_driver_install(dshot_tx_rmt_config.channel, 0, 0);

	if (ret == ESP_OK) {
		 // only do installation once
		has_inited = true;
		has_deinited = false;
		encode_dshot_to_rmt(DSHOT_NULL_PACKET); // cache default timings
		#ifdef DSHOTC3_DEBUG_INIT
		DBGLN("Dshot RMT installed");
		#endif
		return true;
	}
	else {
		DBGLN("Dshot RMT install failed - err %d", ret);
		return false;
	}
}

void DShotRMT::set_looping(bool x) {
	looping = x;
}

// ...the config part is done, now the calculating and sending part
void DShotRMT::send_dshot_value(uint16_t throttle_value, telemetric_request_t telemetric_request) {
	if (throttle_value == 0) {
		next_packet.throttle_value = 0;
	} else {
		if (throttle_value < DSHOT_THROTTLE_MIN) {
			throttle_value = DSHOT_THROTTLE_MIN;
		}

		if (throttle_value > DSHOT_THROTTLE_MAX) {
			throttle_value = DSHOT_THROTTLE_MAX;
		}

		// ...packets are the same for bidirectional mode
		next_packet.throttle_value = throttle_value;
	}
	
	next_packet.telemetric_request = telemetric_request;
	next_packet.checksum = this->calc_dshot_chksum(next_packet);

	has_new_data = true;
	// only cache the next packet but don't need to encode it or send it yet, it'll be sent later
}

rmt_item32_t* DShotRMT::encode_dshot_to_rmt(uint16_t parsed_packet) {
	dshot_tx_rmt_item[DSHOT_PAUSE_BIT].duration1 = 0;
	dshot_tx_rmt_item[DSHOT_PAUSE_BIT].duration0 = 10000 - (16*ticks_per_bit) - 1;

	if (bidirectional) {
		dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = HIGH; // ...pause "bit" added to each frame
		dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = HIGH;
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
		dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level0 = LOW;
		dshot_tx_rmt_item[DSHOT_PAUSE_BIT].level1 = LOW;
	
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
void DShotRMT::output_rmt_data() {
	rmt_tx_stop(rmt_channel);
	set_pin();
	encode_dshot_to_rmt(prepare_rmt_data(next_packet));
	rmt_fill_tx_items(rmt_channel, dshot_tx_rmt_item, DSHOT_PACKET_LENGTH, 0);
	rmt_tx_start(rmt_channel, true);
	has_new_data = false;
}

void DShotRMT::set_pin() {
	if (prev_pin >= 0) { // if there was a previously used pin that needs to be decoupled from RMT
		// rmt_set_pin and rmt_config only seems to add pins to the RMT mux
		// they do not remove pins
		// so we must manually remove the previous pin from the mux
		pinMode(prev_pin, INPUT);
		if (bidirectional == false) {
			digitalWrite(prev_pin, LOW);
			pinMode(prev_pin, OUTPUT);
		}
	}
	pinMode(gpio_num, OUTPUT);
	rmt_set_gpio(rmt_channel, RMT_MODE_TX, gpio_num, false);
	rmt_set_idle_level(rmt_channel, bidirectional ? false : true, bidirectional ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW);
	prev_pin = gpio_num;
}

void DShotRMT::poll() {
	if (cur_node == NULL) { // no instances initalized, do nothing
		return;
	}
	static uint32_t last_time_us = 0;
	uint32_t now_us = micros();
	if ((now_us - last_time_us) < 500) {
		// make sure enough time has passed (RMT driver does not indicate end of transmission at the right time)
		// the number 500 is found by using a logic analyzer to see how often a dshot packet gets cut off
		return;
	}
	rmt_channel_status_result_t status;
	if (rmt_get_channel_status(&status) != ESP_OK || status.status[rmt_channel] != RMT_CHANNEL_IDLE) {
		// make sure RMT is actually idle
		return;
	}
	DShotRMT* inst = cur_node;
	cur_node = (DShotRMT*)inst->next_node; // cycle through linked list
	if (inst->has_new_data || //  send if required
		(inst->looping && (now_us - inst->last_send_time) >= 2000) // limit looping speed
		) {
		inst->output_rmt_data(); // actually send the data, this will set the pin first, and clear the has_new_data flag
		inst->last_send_time = now_us;
		last_time_us = now_us;
	}
}

#endif
