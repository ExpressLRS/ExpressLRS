//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//
#include "devIRTransponder.h"
#include "Transponder.h"
#include "TransponderRMT.h"
#include "RobitronicTransponder.h"
#include "ILapTransponder.h"
#include "options.h"
#include "config.h"
#include "random.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

static TransponderSystem *transponder = nullptr;
static eIRProtocol lastIRProtocol = IRPROTOCOL_NONE;

TransponderRMT transponderRMT;

static void deinitProtocol(eIRProtocol IRprotocol) {
    if (transponder != nullptr) {
        // nothing to do if no protocol is selected
        if (IRprotocol == IRPROTOCOL_NONE)
            return;

        delete transponder;
        transponder = nullptr;
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
        switch (IRprotocol)
        {
            case IRPROTOCOL_ROBITRONIC:
                transponder = new RobitronicTransponder(&transponderRMT);
                break;
            case IRPROTOCOL_ILAP:
                transponder = new ILapTransponder(&transponderRMT);
                break;
            default:
                break;
        }

        if (transponder) {
            transponder->init();
        }

    } else {
        // if no protocl is selected try it again in 100ms
        if (IRprotocol == IRPROTOCOL_NONE) {
            return 100;
        }

        if (transponder) {
            transponder->startTransmission();
        }

        // TODO move this into the transponder API
        switch (IRprotocol)
        {
            case IRPROTOCOL_ROBITRONIC:
                // random wait between 0,5mm and 4,5ms
                return (rngN(5) + 1);
                break;
            case IRPROTOCOL_ILAP:
                return (10); // TODO
                break;
            default:
                break;
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