#include "tx_driver.h"
#include "targets.h"

#include "common.h"
#include "LED.h"

void TxInitSerial()
{
#if defined(TARGET_TX_GHOST)
  Serial.setTx(PA2);
  Serial.setRx(PA3);
#endif

  // what is this? CRSF baud rate?
  Serial.begin(460800);

#if defined(TARGET_TX_FM30)
  pinMode(GPIO_PIN_UART3RX_INVERT, OUTPUT); // RX3 inverter (from radio)
  digitalWrite(GPIO_PIN_UART3RX_INVERT, LOW); // RX3 not inverted
  pinMode(GPIO_PIN_BLUETOOTH_EN, OUTPUT); // Bluetooth enable (disabled)
  digitalWrite(GPIO_PIN_BLUETOOTH_EN, HIGH);
  pinMode(GPIO_PIN_UART1RX_INVERT, OUTPUT); // RX1 inverter (TX handled in CRSF)
  digitalWrite(GPIO_PIN_UART1RX_INVERT, HIGH);
#endif
}

void TxInitLeds()
{
  /**
   * Any TX's that have the WS2812 LED will use this the WS2812 LED pin
   * else we will use GPIO_PIN_LED_GREEN and _RED.
   **/
  #if WS2812_LED_IS_USED // do startup blinkies for fun
      WS281Binit();
      uint32_t col = 0x0000FF;
      for (uint8_t j = 0; j < 3; j++)
      {
          for (uint8_t i = 0; i < 5; i++)
          {
              WS281BsetLED(col << j*8);
              delay(15);
              WS281BsetLED(0, 0, 0);
              delay(35);
          }
      }
      WS281BsetLED(0xff, 0, 0);
  #endif

  #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
  #endif // GPIO_PIN_LED_GREEN
  #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_RED, OUTPUT);
    digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
  #endif // GPIO_PIN_LED_RED

#if defined(TARGET_TX_FM30)
  pinMode(GPIO_PIN_LED_RED_GREEN, OUTPUT); // Green LED on "Red" LED (off)
  digitalWrite(GPIO_PIN_LED_RED_GREEN, HIGH);
  pinMode(GPIO_PIN_LED_GREEN_RED, OUTPUT); // Red LED on "Green" LED (off)
  digitalWrite(GPIO_PIN_LED_GREEN_RED, HIGH);
#endif

#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
  strip.Begin();
#endif
}

void TxInitBuzzer()
{
  #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
    pinMode(GPIO_PIN_BUZZER, OUTPUT);
    // Annoying startup beeps
    #ifndef JUST_BEEP_ONCE
      #if defined(MY_STARTUP_MELODY_ARR)
        // It's silly but I couldn't help myself. See: BLHeli32 startup tones.
        const int melody[][2] = MY_STARTUP_MELODY_ARR;

        for(uint i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
          tone(GPIO_PIN_BUZZER, melody[i][0], melody[i][1]);
          delay(melody[i][1]);
          noTone(GPIO_PIN_BUZZER);
        }
      #else
        // use default jingle
        const int beepFreq[] = {659, 659, 523, 659, 783, 392};
        const int beepDurations[] = {300, 300, 100, 300, 550, 575};

        for (int i = 0; i < 6; i++)
        {
          tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
          delay(beepDurations[i]);
          noTone(GPIO_PIN_BUZZER);
        }
      #endif
    #else
      tone(GPIO_PIN_BUZZER, 400, 200);
      delay(200);
      tone(GPIO_PIN_BUZZER, 480, 200);
    #endif
  #endif // GPIO_PIN_BUZZER
}

void TxInitButton()
{
#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
    button.init(GPIO_PIN_BUTTON, !GPIO_BUTTON_INVERTED); // r9 tx appears to be active high
#endif
}

void TxUpdateLEDs(uint8_t isRXconnected, uint8_t tlm)
{
#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
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
