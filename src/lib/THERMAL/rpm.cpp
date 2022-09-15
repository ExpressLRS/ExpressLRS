#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "logging.h"
#include <driver/pcnt.h>
#include <soc/pcnt_struct.h>

static volatile int overflow = 0;
static uint32_t lastTime = 0;

static void IRAM_ATTR pcnt_overflow(void *arg)
{
    overflow++;
    PCNT.int_clr.val = BIT(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
}

void init_rpm_counter(int pin)
{
    pcnt_config_t pcnt_config = {};
    pcnt_config.pulse_gpio_num = pin;
    pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    // What to do on the positive / negative edge of pulse input?
    pcnt_config.pos_mode = PCNT_COUNT_INC;   // Count up on the positive edge
    pcnt_config.neg_mode = PCNT_COUNT_DIS;   // Keep the counter value on the negative edge
    // Set the maximum and minimum limit values to watch
    pcnt_config.counter_h_lim = 30000;
    pcnt_config.counter_l_lim = -1;
    pcnt_config.unit = PCNT_UNIT_0;
    pcnt_config.channel = PCNT_CHANNEL_0;

    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);
    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_UNIT_0, 100);
    pcnt_filter_enable(PCNT_UNIT_0);
    /* Enable interrupt on overflow */
    pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_H_LIM);
    pcnt_isr_register(pcnt_overflow, NULL, 0, NULL);
    pcnt_intr_enable(PCNT_UNIT_0);
    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT_0);
}

uint32_t get_rpm()
{
    int16_t counter;
    pcnt_get_counter_value(PCNT_UNIT_0, &counter);
    pcnt_counter_clear(PCNT_UNIT_0);
    uint32_t now = millis();
    uint32_t rpm = ((overflow * 30000) + counter) * 60000 / 4 / (now - lastTime);
    lastTime = now;
    return rpm;
}
#endif