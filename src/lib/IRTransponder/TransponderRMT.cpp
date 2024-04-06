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

    DBGLN("TransponderRMT::encode, rmtItemCount: %d", rmtItemCount);
}

void TransponderRMT::init(uint32_t desired_resolution_hz, uint32_t carrier_hz, uint8_t carrier_duty) {

    rmtItemCount = 0;

    rmt_config_t config;

    uint8_t divider = APB_CLK_FREQ / desired_resolution_hz;
    resolutionHz = APB_CLK_FREQ / divider;

    config.channel = channel;
    config.rmt_mode = RMT_MODE_TX;
    config.gpio_num = gpio;
    config.mem_block_num = 1;
    config.clk_div = divider;
    config.tx_config.loop_en = false;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.tx_config.idle_output_en = true;

    if (carrier_hz == 0) {
        config.tx_config.carrier_en = false;
    } else {
        config.tx_config.carrier_en = true;
        config.tx_config.carrier_duty_percent = carrier_duty;
        config.tx_config.carrier_freq_hz = carrier_hz;
        config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    }

    esp_err_t err;
    err = rmt_config(&config);
    if (err != ESP_OK) {
        DBGLN("TransponderRMT::init, config failed for, channel: %d", channel);
        return;
    }
    err = rmt_driver_install(channel, 0, 0);
    if (err != ESP_OK) {
        DBGLN("TransponderRMT::init, driver install failed for, channel: %d", channel);
        return;
    }


    DBGLN("TransponderRMT::init, channel: %d, desired_resolution_hz: %d, actual_resolution_hz: %d, divider: %d, carrier_hz: %d, carrier_duty: %d", channel, desired_resolution_hz, resolutionHz, divider, carrier_hz, carrier_duty);
}

void TransponderRMT::deinit() {
    DBGLN("TransponderRMT::deinit, channel: %d", channel);
    rmt_driver_uninstall(channel);
}

void TransponderRMT::start() {
    if (rmt_wait_tx_done(channel, 0) == ESP_OK)
    {
        rmt_write_items(channel, rmtItems, rmtItemCount, false);
    }
}

#endif
