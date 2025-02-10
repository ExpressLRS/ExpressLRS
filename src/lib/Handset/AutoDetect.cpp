#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_TX)

#include "AutoDetect.h"
#include "CRSFHandset.h"
#include "PPMHandset.h"
#include "logging.h"

#include <driver/rmt.h>

constexpr auto RMT_TICKS_PER_US = 4;

void AutoDetect::Begin()
{
    constexpr auto divisor = 80 / RMT_TICKS_PER_US;

    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(static_cast<gpio_num_t>(GPIO_PIN_RCSIGNAL_RX), PPM_RMT_CHANNEL);
    rmt_rx_config.clk_div = divisor;
    rmt_rx_config.rx_config.filter_ticks_thresh = 1;
    rmt_rx_config.rx_config.idle_threshold = 100;
    rmt_config(&rmt_rx_config);
    rmt_driver_install(PPM_RMT_CHANNEL, 1000, 0);

    rmt_get_ringbuf_handle(PPM_RMT_CHANNEL, &rb);
    rmt_rx_start(PPM_RMT_CHANNEL, true);
    input_detect = 0;
}

void AutoDetect::End()
{
    rmt_driver_uninstall(PPM_RMT_CHANNEL);
}

bool AutoDetect::IsArmed()
{
    return false;
}

void AutoDetect::migrateTo(Handset *that) const
{
    that->setRCDataCallback(RCdataCallback);
    that->registerParameterUpdateCallback(RecvParameterUpdate);
    that->registerCallbacks(connected, disconnected, RecvModelUpdate, OnBindingCommand);
    that->Begin();
    that->setPacketInterval(RequestedRCpacketInterval);
    delete this;
    handset = that;
}

void AutoDetect::startPPM() const
{
    migrateTo(new PPMHandset());
}

void AutoDetect::startCRSF() const
{
    migrateTo(new CRSFHandset());
}

void AutoDetect::handleInput()
{
    size_t length = 0;
    const auto now = millis();

    const auto items = static_cast<rmt_item32_t *>(xRingbufferReceive(rb, &length, 0));
    if (items)
    {
        vRingbufferReturnItem(rb, static_cast<void *>(items));
        lastDetect = now;
        length /= 4; // one RMT = 4 Bytes
        if (length == 0)
        {
            input_detect++;
            if (input_detect > 100)
            {
                //DBGLN("PPM signal detected");
                rmt_driver_uninstall(PPM_RMT_CHANNEL);
                startPPM();
            }
        }
        else
        {
            input_detect--;
            if (input_detect < -100)
            {
                //DBGLN("Serial signal detected");
                rmt_driver_uninstall(PPM_RMT_CHANNEL);
                startCRSF();
            }
        }
    }
    else
    {
        if (now - 1000 > lastDetect && input_detect != 0)
        {
            //DBGLN("No signal detected");
            input_detect = 0;
        }
    }
}

#endif
