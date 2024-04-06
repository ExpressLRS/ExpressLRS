//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//

#if defined(PLATFORM_ESP32)

#include <Arduino.h>
#include "logging.h"
#include "TransponderRMT.h"

void TransponderRMT::configurePeripheral(rmt_channel_t channel, gpio_num_t gpio) {
    this->gpio = gpio;
    this->channel = channel;
}

void TransponderRMT::encode(EncoderRMT *encoder) {

    rmtItemCount = 0;
    while (rmtItemCount < TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE) {
        bool done = encoder->encode_bit(&rmtItems[rmtItemCount]);
        rmtItemCount++;
        if (done) {
            break;
        }
    }

    if (rmtItemCount > 0) {
        state = TRMT_STATE_READY;
    }

    DBGLN("TransponderRMT::encode, rmtItemCount: %d", rmtItemCount);
}

bool TransponderRMT::init(uint32_t desired_resolution_hz, uint32_t carrier_hz, uint8_t carrier_duty) {

    rmtItemCount = 0;
    state = TRMT_STATE_RETRY_INIT;

    uint8_t divider = APB_CLK_FREQ / desired_resolution_hz;
    resolutionHz = APB_CLK_FREQ / divider;

  	rmt_config_t rmt_transponder_config = {
		.rmt_mode = RMT_MODE_TX,
		.channel = channel,
		.gpio_num = gpio,
		.clk_div = divider,
		.mem_block_num = 1,
		.tx_config = {
            .carrier_freq_hz = carrier_hz,
            .carrier_level = RMT_CARRIER_LEVEL_HIGH,
        	.idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = carrier_duty,
			.carrier_en = carrier_hz > 0,
			.loop_en = false,
			.idle_output_en = true,
		},
	};

    esp_err_t err;
    err = rmt_config(&rmt_transponder_config);
    if (err != ESP_OK) {
        DBGLN("TransponderRMT::init, config failed for, channel: %d, error: %d", channel, err);
        return false;
    }
    err = rmt_driver_install(channel, 0, 0);
    if (err != ESP_OK) {
        DBGLN("TransponderRMT::init, driver install failed for, channel: %d, error: %d", channel, err);
        switch (err) {
            case ESP_ERR_INVALID_STATE: {
                rmt_driver_uninstall(channel);
                break;
            default:
                break;
            }
        }
        return false;
    }

    DBGLN("TransponderRMT::init, channel: %d, desired_resolution_hz: %d, actual_resolution_hz: %d, divider: %d, carrier_hz: %d, carrier_duty: %d", channel, desired_resolution_hz, resolutionHz, divider, carrier_hz, carrier_duty);
    state = TRMT_STATE_INIT;
    return true;
}

void TransponderRMT::deinit() {
    DBGLN("TransponderRMT::deinit, channel: %d", channel);
    rmt_driver_uninstall(channel);
    state = TRMT_STATE_UNINIT;
}

void TransponderRMT::start() {
    if (state != TRMT_STATE_READY) {
        return;
    }

    if (rmt_wait_tx_done(channel, 0) == ESP_OK)
    {
        rmt_write_items(channel, rmtItems, rmtItemCount, false);
    }
}

#endif
