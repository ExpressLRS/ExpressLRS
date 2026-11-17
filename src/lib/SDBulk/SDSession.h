#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include "SDBulkLink.h"
#include "crsf_protocol.h"

// A session where the handset lends us its SD card (/ELRS/ only) over the CRSF port.
//
//   TX  -- SD_REQUEST (in-band CRSF, repeated) -->  handset
//   TX  <--------------- SD_OFFER -----------------  handset
//   TX  -- SD_ACCEPT ----------------------------->  handset, both switch to bulk
//   ... SDBulkLink requests from a job task ...
//   TX  -- END ----------------------------------->  handset, both back to CRSF
//
// The job runs in its own task and blocks on the link; the handset device keeps
// calling CRSFHandset::handleInput(), which stands aside while connectionState is
// sdBulk and puts CRSF back together once the job is done.

namespace SDSession
{
enum Subtype : uint8_t
{
    SUB_REQUEST = 0x01,
    SUB_OFFER = 0x02,
    SUB_ACCEPT = 0x03,
    SUB_REJECT = 0x04,
    SUB_STATUS = 0x10,  // TX -> USB/Lua: human-readable status line
};

enum Kind : uint8_t
{
    KIND_RX_UPDATE = 0x01,
    KIND_IMAGE_SYNC = 0x02,
    KIND_TEST = 0x7F,       // bench: read a file, hash it, log the hash
};

void setTransport(SDBulkTransport *transport);

// Ask the handset for a session (Lua command, USB). `arg` is the job's argument:
// card path for RX_UPDATE/TEST, manifest URL for IMAGE_SYNC; may be empty.
// Status lines go to `reportTo` when it isn't the broadcast address.
bool requestSession(uint8_t kind, const char *arg, crsf_addr_e reportTo = CRSF_ADDRESS_BROADCAST);
bool busy();
// short human-readable state ("Waiting for radio", the last result, ...) and when the
// last session finished (0 if none yet)
const char *statusText();
uint32_t finishedAt();

// A CRSF_FRAMETYPE_SD_SESSION frame for the TX module, from the handset or from USB
void handleCrsf(const crsf_ext_header_t *message);

// main loop: request retries and timeouts, status reports
void loop(uint32_t now);

// called by CRSFHandset::handleInput()
bool wantsPort();    // ACCEPT queued; take the port once the output is idle
void portAcquired(); // the port is ours: start the job
bool portReleased(); // the job is done with the port
void portRestored(); // CRSF is back in charge
}

#endif
