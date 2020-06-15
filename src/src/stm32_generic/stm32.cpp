#include "platform.h"
#include "targets.h"
#include "debug_elrs.h"
#include "common.h"
#include "POWERMGNT.h"
#include <Arduino.h>
#include <EEPROM.h>

#define NEW_BUTTON 1

uint8_t rate_config_dips = 0xff;

#if (GPIO_PIN_LED != UNDEF_PIN)
#if defined(TARGET_R9M_RX) || defined(TARGET_R9M_TX)
// Led is inverted
#define LED_STATE_RED(_x) digitalWrite(GPIO_PIN_LED, (_x))
#else // others
#define LED_STATE_RED(_x) digitalWrite(GPIO_PIN_LED, !(_x))
#endif
#else
#define LED_STATE_RED(_x) (void)(_x);
#endif

#if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
#define LED_STATE_GREEN(_x) digitalWrite(GPIO_PIN_LED_GREEN, (_x))
#else
#define LED_STATE_GREEN(_x) (void)(_x);
#endif

#if (GPIO_PIN_BUZZER != UNDEF_PIN)
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
extern POWERMGNT PowerMgmt;

static uint32_t tone_play_cnt = 0;
static uint32_t tone_last_played = 0;
void play_tone_loop(uint32_t ms)
{
    if (tone_play_cnt && 350 < (ms - tone_last_played))
    {
        PLAY_SOUND(244, 50);
        tone_play_cnt--;
        tone_last_played = ms;
    }
}
#endif /* TARGET_R9M_TX */

#if (GPIO_PIN_BUTTON != UNDEF_PIN)
#if defined(TX_MODULE) && NEW_BUTTON
#include "ClickButton.h"

ClickButton clickButton(GPIO_PIN_BUTTON, LOW, 40,  400,  600);

void button_handle(void)
{
    clickButton.update();
    if (clickButton.clicks == 1 && clickButton.lastClickLong) {
        tone_play_cnt = PowerMgmt.loopPower() + 1;
        clickButton.reset();
    } else if (clickButton.clicks == 2 && clickButton.lastClickLong) {
        extern int8_t tx_tlm_toggle(void);
        tone_play_cnt = tx_tlm_toggle() + 1;
        clickButton.reset();
    }
}

#else // NEW_BUTTON

#define TX_CHANGE_POWER_TIME 2000

#include "button.h"
Button button;

void button_handle(void)
{
    button.handle();
}

void button_event_short(uint32_t ms)
{
    (void)ms;
#if defined(TX_MODULE)
#elif defined(RX_MODULE)
    forced_start();
#endif /* RX_MODULE */
}

void button_event_long(uint32_t ms)
{
#if defined(TX_MODULE)
    if (ms > TX_CHANGE_POWER_TIME)
    {
        uint8_t val = PowerMgmt.loopPower() + 1;
        tone_play_cnt = val;
    }
#elif defined(RX_MODULE)
    if (ms > BUTTON_RESET_INTERVAL_RX)
        platform_restart();
#endif /* RX_MODULE */
}

#endif // NEW_BUTTON
#endif // GPIO_PIN_BUTTON

/******************* CONFIG *********************/
int8_t platform_config_load(struct platform_config &config)
{
#if STORE_TO_FLASH
    int8_t res = -1;
    struct platform_config temp;
    EEPROM.get(0, temp);
    if (temp.key == ELRS_EEPROM_KEY)
    {
        /* load ok, copy values */
        config.key = temp.key;
        config.mode = temp.mode;
        config.power = temp.power;
        config.tlm = temp.tlm;
        res = 0;
    }
#else
    int8_t res = 0;
    config.key = ELRS_EEPROM_KEY;
#endif
    if (rate_config_dips < RATE_MAX)
        config.mode = rate_config_dips;
    return res;
}

int8_t platform_config_save(struct platform_config &config)
{
    if (config.key != ELRS_EEPROM_KEY)
        return -1;
#if STORE_TO_FLASH
    EEPROM.put(0, config);
#endif
    return 0;
}

/******************* SETUP *********************/
void platform_setup(void)
{
    EEPROM.begin();

#if defined(DEBUG_SERIAL)
    // Init debug serial if not done already
    if (((void *)&DEBUG_SERIAL != (void *)&CrsfSerial) &&
        ((void *)&DEBUG_SERIAL != (void *)&Serial1))
    {
        // init debug serial
#if (GPIO_PIN_DEBUG_TX != UNDEF_PIN)
        DEBUG_SERIAL.setTx(GPIO_PIN_DEBUG_TX);
#endif
#if (GPIO_PIN_DEBUG_RX != UNDEF_PIN)
        DEBUG_SERIAL.setRx(GPIO_PIN_DEBUG_RX);
#endif
        DEBUG_SERIAL.begin(400000);
    }
#endif /* DEBUG_SERIAL */

#if defined(CTRL_SERIAL)
    CTRL_SERIAL.begin(460800);
    CTRL_SERIAL.setTimeout(5);
#endif // CTRL_SERIAL

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
#if (GPIO_PIN_LED != UNDEF_PIN)
    pinMode(GPIO_PIN_LED, OUTPUT);
    LED_STATE_RED(LOW);
#endif
#if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    LED_STATE_GREEN(LOW);
#endif

#if (GPIO_PIN_BUTTON != UNDEF_PIN)
    /*************** CONFIGURE BUTTON *******************/
#if defined(TX_MODULE) && NEW_BUTTON
#else // NEW_BUTTON
    //button.set_press_delay_short();
    //button.buttonShortPress = button_event_short;
    button.buttonLongPress = button_event_long;
#if defined(TX_MODULE)
    button.set_press_delay_long(TX_CHANGE_POWER_TIME, 1000);
#elif defined(RX_MODULE)
    button.set_press_delay_long(BUTTON_RESET_INTERVAL_RX, 1000);
#endif
    // R9M TX appears to be active high
    button.init(GPIO_PIN_BUTTON, true);
#endif // NEW_BUTTON
#endif

#if defined(TX_MODULE)
    /*************** CONFIGURE TX *******************/

#if defined(TARGET_R9M_TX)
    r9dac.init(GPIO_PIN_SDA, GPIO_PIN_SCL, 0b0001100, GPIO_PIN_RFswitch_CONTROL,
               GPIO_PIN_RFamp_APC1); // used to control ADC which sets PA output
#endif // TARGET_R9M_TX
#if (GPIO_PIN_BUZZER != UNDEF_PIN)
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

#elif defined(RX_MODULE)
    /*************** CONFIGURE RX *******************/

#endif /* RX_MODULE */

#if defined(GPIO_SELECT_RFIO_HIGH) && defined(GPIO_SELECT_RFIO_LOW)
    pinMode(GPIO_SELECT_RFIO_HIGH, OUTPUT);
    pinMode(GPIO_SELECT_RFIO_LOW, OUTPUT);

#if defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    digitalWrite(GPIO_SELECT_RFIO_HIGH, 0);
    digitalWrite(GPIO_SELECT_RFIO_LOW, 1);
#else
    digitalWrite(GPIO_SELECT_RFIO_LOW, 0);
    digitalWrite(GPIO_SELECT_RFIO_HIGH, 1);
#endif
#endif /* RFIO HIGH / LOW */
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
#if (GPIO_PIN_BUTTON != UNDEF_PIN)
#if defined(TX_MODULE)
    button_handle();
    play_tone_loop(millis());
#elif defined(RX_MODULE)
    if (state == STATE_disconnected)
        button.handle();
#endif /* RX_MODULE */
#endif // GPIO_PIN_BUTTON
}

void platform_connection_state(int state)
{
    bool connected = (state == STATE_connected);
    LED_STATE_GREEN(connected ? HIGH : LOW);
#if defined(TX_MODULE)
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
