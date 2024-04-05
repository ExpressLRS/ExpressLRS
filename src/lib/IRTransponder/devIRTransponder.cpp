#include "devIRTransponder.h"
#include "Transponder.h"
#include "options.h"
#include "random.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

static Transponder *transponder = nullptr;

static int start()
{
    // nothing to do if pin not defined
    if (GPIO_IR_TRANSPONDER == UNDEF_PIN)
        return DURATION_NEVER;

    // generate unique transpoder ID (24Bit)
    uint32_t transponderID = ((uint32_t)firmwareOptions.uid[0] << 16) + 
                             ((uint32_t)firmwareOptions.uid[1] << 8) + 
                             ((uint32_t)firmwareOptions.uid[2]);

    // init transponder
    transponder = new Transponder;
    //transponder->init(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER, 80175);    // for testing
    transponder->init(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER, transponderID);

    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    /*
    if (connectionState >= connected)
        return DURATION_NEVER;
    */

    // transmit transponder ID
    transponder->startTransmission();

    // random wait between 0,5mm and 4,5ms
    return (rngN(5) + 1);
}

device_t ir_transponder_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

#endif