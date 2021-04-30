#include "tx_driver.h"
#include "targets.h"

#include "common.h"
#include "LED.h"

#ifdef PLATFORM_ESP32
#include "ESP32_WebUpdate.h"
#endif

void TxInitSerial(HardwareSerial& port, uint32_t baudRate)
{
#if defined(TARGET_TX_GHOST)
  Serial.setTx(PA2);
  Serial.setRx(PA3);

  USART1->CR1 &= ~USART_CR1_UE;
  USART1->CR3 |= USART_CR3_HDSEL;
  USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inv
  USART1->CR1 |= USART_CR1_UE;
#endif

#if defined(TARGET_TX_FM30)
  pinMode(GPIO_PIN_UART3RX_INVERT, OUTPUT); // RX3 inverter (from radio)
  digitalWrite(GPIO_PIN_UART3RX_INVERT, LOW); // RX3 not inverted
  pinMode(GPIO_PIN_BLUETOOTH_EN, OUTPUT); // Bluetooth enable (disabled)
  digitalWrite(GPIO_PIN_BLUETOOTH_EN, HIGH);
  pinMode(GPIO_PIN_UART1RX_INVERT, OUTPUT); // RX1 inverter (TX handled in CRSF)
  digitalWrite(GPIO_PIN_UART1RX_INVERT, HIGH);
#endif

#if defined(PLATFORM_STM32)

  TxRCSerialListenMode();

  // Already done when constructing CRSF_Port
  //
  //CRSF_Port.setTx(GPIO_PIN_RCSIGNAL_TX);
  //CRSF_Port.setRx(GPIO_PIN_RCSIGNAL_RX);

  port.begin(baudRate, SERIAL_8N1);

#endif // endif defined(PLATFORM_STM32)  
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

void TxLEDShowRate(expresslrs_RFrates_e rate)
{
#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
  switch (rate) {
  case RATE_200HZ:
    strip.ClearTo(RgbColor(0, 0, LED_MAX_BRIGHTNESS));
    break;
  case RATE_100HZ:
    strip.ClearTo(RgbColor(0, LED_MAX_BRIGHTNESS, 0));
    break;
  case RATE_50HZ:
    strip.ClearTo(RgbColor(LED_MAX_BRIGHTNESS, 0, 0));
    break;

  default:
    // missing rates:
    // - RATE_500HZ
    // - RATE_250HZ
    // - RATE_150HZ
    // - RATE_25HZ
    // - RATE_4HZ
    break;
  }
  strip.Show();
#endif
}

void TxSetLEDGreen(uint8_t value)
{
#if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
  digitalWrite(GPIO_PIN_LED_GREEN, value ^ GPIO_LED_GREEN_INVERTED);
#endif // GPIO_PIN_LED_GREEN  
}

void TxSetLEDRed(uint8_t value)
{
#if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
  digitalWrite(GPIO_PIN_LED_RED, value ^ GPIO_LED_RED_INVERTED);
#endif // GPIO_PIN_LED_RED
}

void TxBuzzerPlay(unsigned int freq, unsigned long duration)
{
#if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
  tone(GPIO_PIN_BUZZER, freq, duration);
#endif // GPIO_PIN_BUZZER
}

void TxHandleRadioInitError()
{
#ifdef PLATFORM_ESP32
  BeginWebUpdate();
  while (1) {
    HandleWebUpdate();
    delay(1);
  }
#endif

  TxSetLEDGreen(LOW);
  TxBuzzerPlay(480, 200);
  TxSetLEDRed(LOW);
  delay(200);

  TxBuzzerPlay(400, 200);
  TxSetLEDRed(HIGH);
  delay(1000);  
}

void TxRCSerialListenMode()
{
  //TODO: does this really work????
#if defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
  pinMode(GPIO_PIN_BUFFER_OE, OUTPUT);
  digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED); // RX mode default
#endif
}
