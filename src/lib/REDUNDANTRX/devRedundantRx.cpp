#include "targets.h"
#include "common.h"
#include "device.h"

#include "devRedundantRx.h"

#define U2TXD 18 // Currently unused.  The reduntant receivers rx pad can be soldered directly to the FC tx pad.
// #define U2RXD 5
#define U2RXD 0

void sendByteToFC(uint8_t data);
bool getLQCurrentIsSet();

static bool packetReceiveDuringLastInterval = true;

static void initialize()
{
    Serial2.begin(420000, SERIAL_8N1, U2RXD, U2TXD);
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (getLQCurrentIsSet())
        packetReceiveDuringLastInterval = true;

    bool redundantRxDataReceived = false;
    while (Serial2.available())
    {
        redundantRxDataReceived = true;
        unsigned char const inByte = Serial2.read();
        if (!packetReceiveDuringLastInterval)
            sendByteToFC(inByte);
    }
    
    if (redundantRxDataReceived)
        packetReceiveDuringLastInterval = false;

    return DURATION_IMMEDIATELY;
}

static int event()
{
    return DURATION_IMMEDIATELY;
}

device_t RedundantRx_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};
