#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include "SDBulkLink.h"

// IMAGE_SYNC job: join WiFi as a station, fetch a release manifest over HTTP and
// fill /ELRS/FIRMWARE/<image-key>/<version>.bin.gz on the handset's card.
//
// manifest.json (URLs in "file" are relative to the manifest's own URL):
//   { "release": "4.1.0",
//     "images": { "esp32-2400": { "version": "4.1.0", "file": "esp32-2400/4.1.0.bin.gz",
//                                 "size": 712345, "sha256": "<64 hex>" }, ... } }
//
// It joins the home network from the firmware options and, by default, fetches the
// manifest CI published for this very build (RX_SD_MANIFEST_URL), so the receivers
// always get images that match the TX. For development, a manifest URL from the
// session request or the first line of /ELRS/WIFI.TXT replaces that default.

bool runImageSync(SDBulkLink &link, const char *manifestUrl, char *result, size_t resultLen);

// The network a sync will join, for the menu; empty if none is set
void imageSyncNetwork(char *ssid, size_t len);

#endif
