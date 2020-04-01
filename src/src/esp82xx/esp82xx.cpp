#include "targets.h"
#include "debug.h"
#include "button.h"
#include <Arduino.h>

//Button button;

void platform_setup(void)
{
    //button.init(GPIO_PIN_BUTTON, true);
}

void platform_loop(bool connected)
{
    (void)connected;
}

void platform_connection_state(bool connected)
{
    (void)connected;
}
