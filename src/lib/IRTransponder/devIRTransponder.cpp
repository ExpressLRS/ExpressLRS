#include "devIRTransponder.h"
#include "TransponderRMT.h"
#include "RobitronicTransponder.h"
#include "options.h"
#include "config.h"
#include "random.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

static void *transponder = nullptr;
static eIRProtocol lastIRProtocol = IRPROTOCOL_NONE;

TransponderRMT transponderRMT;

static void deinitProtocol(eIRProtocol IRprotocol) {
    if (transponder != nullptr) {
        // nothing to do if no protocol is selected
        if (IRprotocol == IRPROTOCOL_NONE)
            return;

        // deinit Robitronic protocol
        if (IRprotocol == IRPROTOCOL_ROBITRONIC)
        {
            ((RobitronicTransponder *)transponder)->deinit();

            delete ((RobitronicTransponder *)transponder);
        
            transponder = nullptr;
            return;
        }
    }
}

static void initialise()
{
    transponderRMT.configurePeripheral(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER);
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    eIRProtocol IRprotocol = config.GetIRProtocol();

    // check if protocol has changed (LUA)
    if (IRprotocol != lastIRProtocol)
    {        
        // deinit the current protocol
        deinitProtocol(IRprotocol);
        
        lastIRProtocol = IRprotocol;

        // init new protocol
        if (IRprotocol == IRPROTOCOL_ROBITRONIC)
        {
            RobitronicTransponder *t = new RobitronicTransponder(&transponderRMT);
            transponder = t;
            t->init();
        }
    } else {
        // if no protocl is selected try it again in 100ms
        if (IRprotocol == IRPROTOCOL_NONE) {
            return 100;
        }

        // run Robitronic protocol
        if (IRprotocol == IRPROTOCOL_ROBITRONIC)
        {
            // transmit robitronicTransponder ID
            ((RobitronicTransponder *)transponder)->startTransmission();

            // random wait between 0,5mm and 4,5ms
            return (rngN(5) + 1);
        }
    }

    return 1000;
}

device_t ir_transponder_device = {
    .initialize = initialise,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

#endif