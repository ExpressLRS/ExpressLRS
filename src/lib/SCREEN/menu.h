#include "targets.h"

#define LONG_PRESSED 0x80

typedef enum fsm_event_e
{
    EVENT_IMMEDIATE, // immediately do this after the entry-function
    EVENT_TIMEOUT,
    EVENT_ENTER,
    EVENT_UP,
    EVENT_DOWN,
    EVENT_LEFT,
    EVENT_RIGHT,
    EVENT_LONG_ENTER = EVENT_ENTER | LONG_PRESSED,
    EVENT_LONG_UP = EVENT_UP | LONG_PRESSED,
    EVENT_LONG_DOWN = EVENT_DOWN | LONG_PRESSED,
    EVENT_LONG_LEFT = EVENT_LEFT | LONG_PRESSED,
    EVENT_LONG_RIGHT = EVENT_RIGHT | LONG_PRESSED
} fsm_event_t;

void startFSM(uint32_t now);
void handleEvent(uint32_t now, fsm_event_t event);
