#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include "SDBulkLink.h"

// RX_UPDATE job. `linked`: the RX has already been told over the RC link to stop and
// listen; otherwise keep calling until it's powered on and hears us during its boot
// window. Find it over ESP-NOW, pick its image off the handset's card (newest under
// /ELRS/FIRMWARE/<its key>/, or `path` if given), inflate if it's .gz, and send it.
// Not linked, a receiver in bind mode will do too (the first to answer): it is shown
// by name for a moment, and binds to us once the image is in.
bool runRxUpdate(SDBulkLink &link, const char *path, bool linked, char *result, size_t resultLen);

// Bind over ESP-NOW, ahead of and alongside the LoRa bind: a receiver in bind mode
// that answers gets our UID straight away, and LoRa bind is skipped or cut short.
namespace RxBind
{
enum State : uint8_t
{
    IDLE,
    RUNNING,
    BOUND, // a receiver took our UID over ESP-NOW
    NONE,  // nobody answered
};
struct Found
{
    char name[17];
    char version[25];
    bool needsUpdate; // not this TX's build
    bool otherMajor;  // can't even link to this TX until it's updated
};
// Bind pressed: start looking
void start();
// LoRa bind waits while this is true: ESP-NOW goes first, for a second
bool holdingLoRa();
// LoRa bind is over: stop looking
void stop();
State state();
const Found &found();
// bound over ESP-NOW recently enough that the RX is still listening on its new MAC
bool justBound();
}

#endif
