#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "platform_internal.h"
#include <stdint.h>

#define ELRS_EEPROM_KEY 0x454c5253 // ELRS

struct platform_config
{
    uint32_t key;
    uint32_t mode;
    uint32_t power;
};

int8_t platform_config_load(struct platform_config &config);
int8_t platform_config_save(struct platform_config &config);
void platform_setup(void);
void platform_mode_notify(void);
void platform_loop(int state);
void platform_connection_state(int state);
void platform_set_led(bool state);
void platform_restart(void);
void platform_wd_feed(void);

#endif // _PLATFORM_H_
