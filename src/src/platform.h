#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "platform_internal.h"
#include <stdint.h>

void platform_setup(void);
void platform_loop(int state);
void platform_connection_state(int state);
void platform_set_led(bool state);
void platform_restart(void);
void platform_wd_feed(void);

#endif // _PLATFORM_H_
