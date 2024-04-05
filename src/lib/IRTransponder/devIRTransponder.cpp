#include "devIRTransponder.h"
#include "RobitronicTransponder.h"
#include "options.h"
#include "config.h"
#include "random.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

static void *transponder = nullptr;
static eIRProtocol lastIRProtocol = IRPROTOCOL_NONE;


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
            // generate unique transpoder ID (24Bit)
            uint32_t transponderID = ((uint32_t)firmwareOptions.uid[0] << 16) + 
                                     ((uint32_t)firmwareOptions.uid[1] << 8) + 
                                     ((uint32_t)firmwareOptions.uid[2]);

            // init robitronicTransponder
            transponder = new RobitronicTransponder;
            //robitronicTransponder->init(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER, 0);    // for testing
            ((RobitronicTransponder *)transponder)->init(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER, 80175);    // for testing
            //robitronicTransponder->init(RMT_CHANNEL_0, (gpio_num_t)GPIO_IR_TRANSPONDER, transponderID);
        }
    } else 
        {   
            // if no protocl is selected try it again in 100ms
            if (IRprotocol == IRPROTOCOL_NONE)
                return 100;

            // run Robitronic protocol
            if (IRprotocol == IRPROTOCOL_ROBITRONIC)
            {
                // transmit robitronicTransponder ID
                ((RobitronicTransponder *)transponder)->startTransmission();

                // random wait between 0,5mm and 4,5ms
                return (rngN(5) + 1);
            }
        }
}

device_t ir_transponder_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

#endif