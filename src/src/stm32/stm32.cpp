#include "targets.h"
#include "debug.h"
#include "common.h"
#include <Arduino.h>

#ifdef GPIO_PIN_BUTTON
#include "button.h"
Button button;

void button_event_short(uint32_t ms)
{
    (void)ms;
#if defined(TARGET_R9M_TX)
#elif defined(TARGET_R9M_RX)
    forced_start();
#endif /* TARGET_R9M_RX */
}

void button_event_long(uint32_t ms)
{
#if defined(TARGET_R9M_TX)
#elif defined(TARGET_R9M_RX)
    if (ms > BUTTON_RESET_INTERVAL_RX)
        platform_restart();
#endif /* TARGET_R9M_RX */
}
#endif

#if defined(TARGET_R9M_TX)
#include "DAC.h"
R9DAC r9dac;

#elif defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */

void platform_setup(void)
{
#if defined(DEBUG_SERIAL)
    if ((void *)&DEBUG_SERIAL != (void *)&CrsfSerial)
    {
        // init debug serial
#ifdef GPIO_PIN_DEBUG_TX
        DEBUG_SERIAL.setTx(GPIO_PIN_DEBUG_TX);
#endif
#ifdef GPIO_PIN_DEBUG_RX
        DEBUG_SERIAL.setRx(GPIO_PIN_DEBUG_RX);
#endif
        DEBUG_SERIAL.begin(400000);
    }
#endif

/*************** CONFIGURE LEDs *******************/
#ifdef GPIO_PIN_LED
    pinMode(GPIO_PIN_LED, OUTPUT);
    digitalWrite(GPIO_PIN_LED, LOW);
#endif
#ifdef GPIO_PIN_LED_GREEN
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, LOW);
#endif

/*************** CONFIGURE BUTTON *******************/
#ifdef GPIO_PIN_BUTTON
    //button.set_press_delay_short();
    //button.buttonShortPress = button_event_short;
    button.buttonLongPress = button_event_long;
    button.set_press_delay_long(BUTTON_RESET_INTERVAL_RX, 1000);
    // R9M TX appears to be active high
    button.init(GPIO_PIN_BUTTON, true);
#endif

/*************** CONFIGURE TX *******************/
#if defined(TARGET_R9M_TX)
    r9dac.init(GPIO_PIN_SDA, GPIO_PIN_SCL, 0b0001100,
               GPIO_PIN_RFswitch_CONTROL,
               GPIO_PIN_RFamp_APC1); // used to control ADC which sets PA output
    //r9dac.setPower(R9_PWR_50mW);

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
    //tone(GPIO_PIN_BUZZER, 400, 200);
    tone(GPIO_PIN_BUZZER, 200, 50);
#endif // JUST_BEEP_ONCE
#endif // GPIO_PIN_BUZZER

/*************** CONFIGURE RX *******************/
#elif defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */
}

void platform_loop(connectionState_e state)
{
    (void)state;
#ifdef GPIO_PIN_BUTTON
    if (state == STATE_disconnected)
        button.handle();
#endif

#if defined(TARGET_R9M_TX)
#elif defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */
}

void platform_connection_state(connectionState_e state)
{
    bool connected = (state == STATE_connected);
#ifdef GPIO_PIN_LED_GREEN
    digitalWrite(GPIO_PIN_LED_GREEN, (connected ? HIGH : LOW));
#endif
#if defined(TARGET_R9M_TX)
    platform_set_led(!connected);
#endif
}

void platform_set_led(bool state)
{
#ifdef GPIO_PIN_LED
    digitalWrite(GPIO_PIN_LED, (uint32_t)state);
#endif
}

void platform_restart(void)
{
    HAL_NVIC_SystemReset();
}
