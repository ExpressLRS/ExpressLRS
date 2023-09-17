#include <Arduino.h>

class RgbColor
{
public:
    RgbColor(uint8_t r, uint8_t g, uint8_t b) : R(r), G(g), B(b) {}
    explicit RgbColor(uint8_t brightness) : R(brightness), G(brightness), B(brightness) {}

    uint8_t R;
    uint8_t G;
    uint8_t B;
};

class ESP32S3LedDriver
{
public:
    ESP32S3LedDriver(int count, int pin);
    ~ESP32S3LedDriver();

    void Begin();
    void Show();
    void ClearTo(RgbColor color, uint16_t first, uint16_t last);
    virtual void SetPixelColor(uint16_t indexPixel, RgbColor color) = 0;

private:
    RgbColor *ledsbuff = nullptr;
    uint16_t *out_buffer = nullptr;
    size_t out_buffer_size;
    int num_leds;
    int gpio_pin;

    friend class ESP32S3LedDriverGRB;
    friend class ESP32S3LedDriverRGB;
};

class ESP32S3LedDriverGRB : public ESP32S3LedDriver
{
public:
    ESP32S3LedDriverGRB(int count, int pin) : ESP32S3LedDriver(count, pin) {}
    void SetPixelColor(uint16_t indexPixel, RgbColor color) override;
};

class ESP32S3LedDriverRGB : public ESP32S3LedDriver
{
public:
    ESP32S3LedDriverRGB(int count, int pin) : ESP32S3LedDriver(count, pin) {}
    void SetPixelColor(uint16_t indexPixel, RgbColor color) override;
};
