#pragma once

#if defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266))

#include <stdint.h>

namespace RxUpdateReceiver
{
// Stop the RC link and wait for the TX over ESP-NOW. Refused while armed.
bool enter();
// main loop while connectionState == rxUpdate
void loop(uint32_t now);
// End of setup(): listen for a TX's HELLO for BOOT_LISTEN_MS while the link starts,
// so an RX that can't link (another major version, say) can still be updated
void bootListen();
// Bind mode started (true) or ended (false): while it lasts, answer a TX's broadcast
// HELLO with our name, and let it update us and bind us to its UID
void bindListen(bool on);
// WiFi (the web UI) is about to take the radio: stop either kind of listening
void stopListening();
// every loop(): a HELLO during the boot window, or a BEGIN after we answered in bind
// mode, enters update mode. The first valid RC packet or the end of the boot window
// turns WiFi back off.
void listenLoop(uint32_t now);
}

#endif
