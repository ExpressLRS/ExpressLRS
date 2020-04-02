#include "targets.h"
#include "debug.h"
#include <Arduino.h>

#if defined(TARGET_R9M_TX)
#include "DAC.h"
#include "button.h"
Button button;
R9DAC r9dac;
#endif
#if defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */

void platform_setup(void)
{
#ifdef DEBUG_SERIAL
    // init debug serial
    DEBUG_SERIAL.setTx(GPIO_PIN_DEBUG_TX);
    DEBUG_SERIAL.setRx(GPIO_PIN_DEBUG_RX);
    DEBUG_SERIAL.begin(115200);
#endif

#if defined(TARGET_R9M_TX)
#ifdef GPIO_PIN_BUZZER
    pinMode(GPIO_PIN_BUZZER, OUTPUT);

#define JUST_BEEP_ONCE 1

#ifndef JUST_BEEP_ONCE
    // Annoying startup beeps
    const int beepFreq[] = {659, 659, 659, 523, 659, 783, 392};
    const int beepDurations[] = {150, 300, 300, 100, 300, 550, 575};

    for (int i = 0; i < 7; i++)
    {
        tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
        delay(beepDurations[i]);
        noTone(GPIO_PIN_BUZZER);
    }
#else  // JUST_BEEP_ONCE
    tone(GPIO_PIN_BUZZER, 400, 200);
#endif // JUST_BEEP_ONCE
#endif // GPIO_PIN_BUZZER

    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    pinMode(GPIO_PIN_LED_RED, OUTPUT);

    digitalWrite(GPIO_PIN_LED_GREEN, LOW);
    digitalWrite(GPIO_PIN_LED_RED, LOW);

    pinMode(GPIO_PIN_RFswitch_CONTROL, OUTPUT);
    pinMode(GPIO_PIN_RFamp_APC1, OUTPUT);
    digitalWrite(GPIO_PIN_RFamp_APC1, HIGH);

    r9dac.init(GPIO_PIN_SDA, GPIO_PIN_SCL, 0b0001100); // used to control ADC which sets PA output
    //r9dac.setPower(R9_PWR_50mW);

    button.init(GPIO_PIN_BUTTON, true); // r9 tx appears to be active high

#endif /* TARGET_R9M_TX */

#if defined(TARGET_R9M_RX)
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, LOW);
#endif /* TARGET_R9M_RX */
}

void platform_loop(bool connected)
{
    (void)connected;

#if defined(TARGET_R9M_TX)
    button.handle();
#endif /* TARGET_R9M_TX */
#if defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */
}

void platform_connection_state(bool connected)
{
#if defined(TARGET_R9M_TX)
    digitalWrite(GPIO_PIN_LED_GREEN, (connected ? HIGH : LOW));
    //digitalWrite(GPIO_PIN_LED_RED, (connected ? LOW : HIGH));
#endif /* TARGET_R9M_TX */
#if defined(TARGET_R9M_RX)
    digitalWrite(GPIO_PIN_LED_GREEN, (connected ? HIGH : LOW));
#endif /* TARGET_R9M_RX */
}
