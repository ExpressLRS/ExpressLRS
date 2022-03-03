#include "targets.h"

typedef enum fsm_event_e
{
    EVENT_IMMEDIATE, // immediately do this after the entry-function
    EVENT_TIMEOUT,
    EVENT_ENTER,
    EVENT_UP,
    EVENT_DOWN,
    EVENT_LEFT,
    EVENT_RIGHT
} fsm_event_t;

void startFSM(uint32_t now);
void handleEvent(uint32_t now, fsm_event_t event);
