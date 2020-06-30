/// LED SUPPORT ///////
#ifdef PLATFORM_ESP32
#include <NeoPixelBus.h>
const uint16_t PixelCount = 1; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 27;   // make sure to set this to the correct pin, ignored for Esp8266
#define colorSaturation 50
NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);
#endif

void updateLEDs(uint8_t isRXconnected, uint8_t tlm)
{
#ifdef PLATFORM_ESP32
    if (ExpressLRS_currAirRate_Modparams->enum_rate == RATE_200HZ)
    {
        strip.ClearTo(RgbColor(0, 0, LED_MAX_BRIGHTNESS));
    }
    if (ExpressLRS_currAirRate_Modparams->enum_rate == RATE_100HZ)
    {
        strip.ClearTo(RgbColor(0, LED_MAX_BRIGHTNESS, 0));
    }
    if (ExpressLRS_currAirRate_Modparams->enum_rate == RATE_50HZ)
    {
        strip.ClearTo(RgbColor(LED_MAX_BRIGHTNESS, 0, 0));
    }
  strip.Show();
#endif
}