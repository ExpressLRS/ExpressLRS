/// LED SUPPORT ///////
#ifdef PLATFORM_ESP32
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
#endif

void updateLEDs(uint8_t isRXconnected, uint8_t tlm)
{
#ifdef PLATFORM_ESP32
  LEDGlowIndex = millis() % 5000;

  if(LEDGlowIndex < 2500)
  {
    LEDGlowIndex = LEDGlowIndex / 10;
  } else
  {
    LEDGlowIndex = 250 - (LEDGlowIndex - 2500) / 10;
  }

  for(int n = 0; n < numberOfLEDs; n++) strip.SetPixelColor(n, RgbColor(LEDGlowIndex, LEDGlowIndex, LEDGlowIndex));

  if(isRXconnected || tlm == 0)
  {
    if(ExpressLRS_currAirRate.enum_rate == RATE_200HZ)
    {
      for(int n = 0; n < numberOfLEDs; n++) strip.SetPixelColor(n, RgbColor(0, 0, LEDGlowIndex));
    }
    if(ExpressLRS_currAirRate.enum_rate == RATE_100HZ)
    {
      for(int n = 0; n < numberOfLEDs; n++) strip.SetPixelColor(n, RgbColor(0, LEDGlowIndex, 0));
    }
    if(ExpressLRS_currAirRate.enum_rate == RATE_50HZ)
    {
      for(int n = 0; n < numberOfLEDs; n++) strip.SetPixelColor(n, RgbColor(LEDGlowIndex, 0, 0));
    }
  }
  strip.Show();
#endif
}