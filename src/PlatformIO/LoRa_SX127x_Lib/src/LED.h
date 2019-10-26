/// LED SUPPORT ///////
#include <NeoPixelBus.h>
const uint16_t PixelCount = 4; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 27;   // make sure to set this to the correct pin, ignored for Esp8266
const uint8_t numberOfLEDs = 3;
uint16_t LEDGlowIndex = 0;
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
//////////////////////////////////