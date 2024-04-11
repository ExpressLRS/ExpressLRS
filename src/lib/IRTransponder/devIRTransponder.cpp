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
static eIRProtocol activeIRProtocol = IRPROTOCOL_NONE;

TransponderRMT transponderRMT;

static void deinitCurrentProtocol() {
    if (transponder == nullptr) {
        return;
    }

    delete transponder;
    transponder = nullptr;
}

static void activateProtocol(eIRProtocol irProtocol) {
    activeIRProtocol = irProtocol;

    switch (irProtocol)
    {
        case IRPROTOCOL_ROBITRONIC:
            transponder = new RobitronicTransponder(&transponderRMT);
            break;
        case IRPROTOCOL_ILAP:
            transponder = new ILapTransponder(&transponderRMT);
            break;
        default:
            return;
    }

    if (transponder) {
        transponder->init();
    }
}

static void initialise()
{
    transponderRMT.configurePeripheral((gpio_num_t)GPIO_IR_TRANSPONDER);
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    eIRProtocol irProtocol = config.GetIRProtocol();

    bool protocolChanged = irProtocol != activeIRProtocol;
    if (protocolChanged)
    {        
        deinitCurrentProtocol();
        activateProtocol(irProtocol);

        return DURATION_IMMEDIATELY;
    } else {
        // if no protocol is active try again in after a short delay
        if (activeIRProtocol == IRPROTOCOL_NONE || !transponder) {
            return 500;
        }

        if (!transponder->isInitialised()) {
            transponder->init();
        }
        transponder->startTransmission();

        // TODO move this into the transponder API
        switch (activeIRProtocol)
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