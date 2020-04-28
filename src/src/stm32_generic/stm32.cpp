#include "platform.h"
#include "targets.h"
#include "debug.h"
#include "common.h"
#include <Arduino.h>
#include <EEPROM.h>

uint8_t rate_config_dips = 0xff;

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
#endif // GPIO_PIN_BUTTON

#ifdef GPIO_PIN_LED
#define LED_STATE_RED(_x) digitalWrite(GPIO_PIN_LED, (_x))
#else
#define LED_STATE_RED(_x)
#endif

#ifdef GPIO_PIN_LED_GREEN
#define LED_STATE_GREEN(_x) digitalWrite(GPIO_PIN_LED_GREEN, (_x))
#else
#define LED_STATE_GREEN(_x)
#endif

#ifdef GPIO_PIN_BUZZER
static inline void PLAY_SOUND(uint32_t wait = 244, uint32_t cnt = 50)
{
    for (uint32_t x = 0; x < cnt; x++)
    {
        // 1 / 2048Hz = 488uS, or 244uS high and 244uS low to create 50% duty cycle
        digitalWrite(GPIO_PIN_BUZZER, HIGH);
        delayMicroseconds(wait);
        digitalWrite(GPIO_PIN_BUZZER, LOW);
        delayMicroseconds(wait);
    }
}
#else
#define PLAY_SOUND(_x, _y)
#endif

#if defined(TARGET_R9M_TX)
#include "DAC.h"
R9DAC r9dac;

#elif defined(TARGET_R9M_RX)
#endif /* TARGET_R9M_RX */

/******************* CONFIG *********************/
int8_t platform_config_load(struct platform_config &config)
{
    EEPROM.get(0, config);
    if (rate_config_dips < RATE_MAX)
        config.mode = rate_config_dips;
    return (config.key == ELRS_EEPROM_KEY) ? 0 : -1;
}

int8_t platform_config_save(struct platform_config &config)
{
    if (config.key != ELRS_EEPROM_KEY)
        return -1;
    EEPROM.put(0, config);
    return 0;
}

/******************* SETUP *********************/
void platform_setup(void)
{
    EEPROM.begin();

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

    /**** SWTICHES ****/
#if defined(GPIO_PIN_DIP1) && defined(GPIO_PIN_DIP2)
    pinMode(GPIO_PIN_DIP1, INPUT_PULLUP);
    pinMode(GPIO_PIN_DIP2, INPUT_PULLUP);
    rate_config_dips = digitalRead(GPIO_PIN_DIP1) ? 0u : 1u;
    rate_config_dips <<= 1;
    rate_config_dips |= digitalRead(GPIO_PIN_DIP2) ? 0u : 1u;
    if (rate_config_dips < RATE_MAX)
    {
        current_rate_config = rate_config_dips;
    }
#endif

    /*************** CONFIGURE LEDs *******************/
#ifdef GPIO_PIN_LED
    pinMode(GPIO_PIN_LED, OUTPUT);
    LED_STATE_RED(LOW);
#endif
#ifdef GPIO_PIN_LED_GREEN
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    LED_STATE_GREEN(LOW);
#endif

#ifdef GPIO_PIN_BUTTON
    /*************** CONFIGURE BUTTON *******************/
    //button.set_press_delay_short();
    //button.buttonShortPress = button_event_short;
    button.buttonLongPress = button_event_long;
    button.set_press_delay_long(BUTTON_RESET_INTERVAL_RX, 1000);
    // R9M TX appears to be active high
    button.init(GPIO_PIN_BUTTON, true);
#endif

#if defined(TARGET_R9M_TX)
    /*************** CONFIGURE TX *******************/

    r9dac.init(GPIO_PIN_SDA, GPIO_PIN_SCL, 0b0001100, GPIO_PIN_RFswitch_CONTROL,
               GPIO_PIN_RFamp_APC1); // used to control ADC which sets PA output

#ifdef GPIO_PIN_BUZZER
    pinMode(GPIO_PIN_BUZZER, OUTPUT);

#define STARTUP_BEEPS 0

#if STARTUP_BEEPS
    // Annoying startup beeps
    const int beepFreq[] = {659, 659, 659, 523, 659, 783, 392};
    const int beepDurations[] = {150, 300, 300, 100, 300, 550, 575};

    for (int i = 0; i < 7; i++)
    {
        tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
        delay(beepDurations[i]);
        noTone(GPIO_PIN_BUZZER);
    }
#endif // STARTUP_BEEPS
#endif // GPIO_PIN_BUZZER

#elif defined(TARGET_R9M_RX)
    /*************** CONFIGURE RX *******************/

#endif /* TARGET_R9M_RX */
}

void platform_mode_notify(void)
{
    for (int i = 0; i < RATE_GET_OSD_NUM(current_rate_config); i++)
    {
        delay(300);
        LED_STATE_GREEN(HIGH);
        PLAY_SOUND(244, 50);
        delay(50);
        LED_STATE_GREEN(LOW);
    }
}

void platform_loop(int state)
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

void platform_connection_state(int state)
{
    bool connected = (state == STATE_connected);
    LED_STATE_GREEN(connected ? HIGH : LOW);
#if defined(TARGET_R9M_TX)
    //platform_set_led(!connected);
#endif
}

void platform_set_led(bool state)
{
    LED_STATE_RED((uint32_t)state);
}

void platform_restart(void)
{
    NVIC_SystemReset();
}

void platform_wd_feed(void)
{
}
