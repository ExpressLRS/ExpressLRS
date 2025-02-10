#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_TX)

#include "PPMHandset.h"
#include "common.h"
#include "crsf_protocol.h"
#include "logging.h"

#include <driver/rmt.h>

constexpr auto RMT_TICKS_PER_US = 4;

void PPMHandset::Begin()
{
    constexpr auto divisor = 80 / RMT_TICKS_PER_US;

    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(static_cast<gpio_num_t>(GPIO_PIN_RCSIGNAL_RX), PPM_RMT_CHANNEL);
    rmt_rx_config.clk_div = divisor;
    rmt_rx_config.rx_config.idle_threshold = min(65000, RMT_TICKS_PER_US * 4000); // greater than 4ms triggers end of frame
    rmt_config(&rmt_rx_config);
    rmt_driver_install(PPM_RMT_CHANNEL, 1000, 0);

    rmt_get_ringbuf_handle(PPM_RMT_CHANNEL, &rb);
    rmt_rx_start(PPM_RMT_CHANNEL, true);
    lastPPM = 0;

    if (connected)
    {
        connected();
    }
}

void PPMHandset::End()
{
    rmt_driver_uninstall(PPM_RMT_CHANNEL);
}

bool PPMHandset::IsArmed()
{
    bool maybeArmed = numChannels < 5 || CRSF_to_BIT(ChannelData[4]);
    return maybeArmed && lastPPM;
}

void PPMHandset::handleInput()
{
    const auto now = millis();
    size_t length = 0;

    auto *items = static_cast<rmt_item32_t *>(xRingbufferReceive(rb, &length, 0));
    if (items)
    {
        length /= 4; // one RMT = 4 Bytes
        int channelCount = 0;
        for (int i = 0; i < length; i++)
        {
            const auto item = items[i];
            // Stop if there is a 0 duration
            if (item.duration0 == 0 || item.duration1 == 0)
            {
                break;
            }
            channelCount ++;
            const auto ppm = (item.duration0 + item.duration1) / RMT_TICKS_PER_US;
            ChannelData[i] = fmap(ppm, 988, 2012, CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX);
        }
        numChannels = channelCount;
        vRingbufferReturnItem(rb, static_cast<void *>(items));
        lastPPM = now;
    }
    else if (lastPPM && now - 1000 > lastPPM)
    {
        //DBGLN("PPM signal lost, disarming");
        if (disconnected)
        {
            disconnected();
        }
        lastPPM = 0;
    }
}

#endif