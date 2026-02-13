#include "targets.h"

#if defined(PLATFORM_ESP32)

#include <cstring>
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp32rgb.h"

constexpr auto I2S_NUM = I2S_NUM_0;

#if defined(CONFIG_IDF_TARGET_ESP32S2)
constexpr uint32_t SAMPLE_RATE = 375000;
constexpr int MCLK = 48000000;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr auto SAMPLE_RATE = 800000U;
constexpr auto MCLK = 160000000;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
constexpr auto SAMPLE_RATE = 800000U;
constexpr auto MCLK = 160000000;
#elif defined(CONFIG_IDF_TARGET_ESP32)
constexpr auto SAMPLE_RATE = 375000U;
constexpr auto MCLK = 48000000;
#endif

constexpr auto DMA_BUF_LEN = 1024;
constexpr auto DMA_BUF_COUNT = 2;
constexpr auto I2S_BITS_PER_RGB_BIT = I2S_BITS_PER_SAMPLE_16BIT;
constexpr auto BYTES_PER_LED = 24 * (I2S_BITS_PER_RGB_BIT / 8);
constexpr auto MAX_LEDS = (DMA_BUF_LEN * DMA_BUF_COUNT - 4) / BYTES_PER_LED;

ESP32LedDriver::ESP32LedDriver(const int count, const int pin) : num_leds(count), gpio_pin(pin)
{
    num_leds = std::min(count, MAX_LEDS);
    out_buffer_size = num_leds * BYTES_PER_LED;
    out_buffer = static_cast<uint16_t *>(heap_caps_malloc(out_buffer_size, MALLOC_CAP_DMA));
    memset(out_buffer, 0, out_buffer_size);
}

ESP32LedDriver::~ESP32LedDriver()
{
    heap_caps_free(out_buffer);
}

void ESP32LedDriver::Begin() const
{
    constexpr i2s_config_t i2s_config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_RGB_BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = MCLK,
    };

    const i2s_pin_config_t pin_config = {
        .bck_io_num = -1,
        .ws_io_num = -1,
        .data_out_num = gpio_pin,
        .data_in_num = -1,
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, nullptr);
    delay(1); // without this it fails to boot and gets stuck!
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM);
    i2s_stop(I2S_NUM);
}

void ESP32LedDriver::Show() const
{
    size_t bytes_written = 0;
    i2s_stop(I2S_NUM);
    if (i2s_write(I2S_NUM, out_buffer, out_buffer_size, &bytes_written, 0) == ESP_OK)
    {
        i2s_start(I2S_NUM);
    }
}

void ESP32LedDriver::ClearTo(const RgbColor color, const int first, const int last)
{
    for (auto i=first ; i<=std::max(last, num_leds-1); i++)
    {
        SetPixelColor(i, color);
    }
}


#if defined(CONFIG_IDF_TARGET_ESP32S2)
static const int bit_order[] = {0x40, 0x80, 0x10, 0x20, 0x04, 0x08, 0x01, 0x02};
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
static const int bit_order[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
static const int bit_order[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
#elif defined(CONFIG_IDF_TARGET_ESP32)
static const int bit_order[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
#endif

void ESP32LedDriverGRB::SetPixelColor(const int indexPixel, const RgbColor color)
{
    if (indexPixel < num_leds)
    {
        const auto loc = indexPixel * 24;
        for(auto bit_pos = 0 ; bit_pos < 8 ; bit_pos++)
        {
            const auto bit = bit_order[bit_pos];
            out_buffer[loc + bit_pos + 0] = (color.G & bit) ? 0xFFE0 : 0xF000;
            out_buffer[loc + bit_pos + 8] = (color.R & bit) ? 0xFFE0 : 0xF000;
            out_buffer[loc + bit_pos + 16] = (color.B & bit) ? 0xFFE0 : 0xF000;
        }
    }
}

void ESP32LedDriverRGB::SetPixelColor(const int indexPixel, const RgbColor color)
{
    if (indexPixel < num_leds)
    {
        const auto loc = indexPixel * 24;
        for(auto bit_pos = 0 ; bit_pos < 8 ; bit_pos++)
        {
            const auto bit = bit_order[bit_pos];
            out_buffer[loc + bit_pos + 0] = (color.R & bit) ? 0xFFE0 : 0xF000;
            out_buffer[loc + bit_pos + 8] = (color.G & bit) ? 0xFFE0 : 0xF000;
            out_buffer[loc + bit_pos + 16] = (color.B & bit) ? 0xFFE0 : 0xF000;
        }
    }
}

#endif