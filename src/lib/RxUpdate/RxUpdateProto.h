#pragma once

#include <stdint.h>
#include <string.h>
#if defined(PLATFORM_ESP32)
#include <esp_wifi_types.h>
#endif

// TX -> RX firmware transfer over ESP-NOW. The RX listens either after it was told
// over the RC link (MSP_ELRS_ENTER_RX_UPDATE) to stop the radio, or for a moment
// after every boot (BOOT_LISTEN_MS), which reaches an RX that can't link at all:
// one a major version ahead of or behind the TX, say.
//
// The RX takes a station MAC derived from the binding UID, so the TX knows where to
// send without any discovery, and only a TX it's bound to can reach it. Only the four
// UID bytes the link itself uses go into it: a bind over the link (MSP) carries just
// those, so a receiver bound that way, with a phrase or over ESP-NOW all get the same.
//
// An RX in bind mode also listens, with its own factory MAC, for a HELLO broadcast by
// any TX: it answers with its name and stays in bind mode. Several receivers in bind
// mode can answer the same HELLO; the TX takes the first. Then either:
//   * BIND(uid): the TX's Bind. The RX takes the UID, leaves bind mode, and listens on
//     its new UID's MAC (as after a boot) until the link is up, so an update offered
//     right after reaches it even when it can't link to this TX.
//   * BEGIN: the TX's Update Receiver. The RX updates, and after a good FINISH binds to
//     the UID from the HELLO.
//
// HELLO/BEGIN/FINISH are stop-and-wait. DATA is windowed and acked cumulatively, though
// not every DATA gets its own answer (see ACK_EVERY).
//
//   TX  HELLO(uid)   ->  RX  HELLO_ACK   (image key, version, commit, max image size, name)
//   TX  BEGIN(size)  ->  RX  BEGIN_ACK
//   TX  DATA(off)    ->  RX  DATA_ACK(next expected offset)   ... a window in flight
//   TX  FINISH(sha)  ->  RX  FINISH_ACK(status, text), then the RX reboots into it
//   TX  CANCEL_UPDATE(why)  ->  RX  CANCEL_UPDATE_ACK, then the RX reboots into what it has
//   TX  BIND(uid)    ->  RX  BIND_ACK    (bind mode only)
//
// This protocol has to outlive the RC link protocol: an RX that can't link is
// exactly the one that needs it. So it only ever grows. Packets keep their types
// and fields, new fields go on the end, and both sides ignore what they don't know.
// PROTO_VERSION in HELLO/HELLO_ACK (0 from the first receivers, which don't send it)
// says what the other end understands.
//
// The image is the bare firmware (no options/hardware trailer); the RX appends its
// own trailer so it keeps its layout, bind phrase and settings.

namespace RxUpdate
{
// 2: HELLO carries the TX's UID, HELLO_ACK the RX's name; bind-mode receivers, BIND
constexpr uint8_t PROTO_VERSION = 2;
constexpr uint8_t ESPNOW_CHANNEL = 1;
// how long an RX listens for a HELLO after boot, alongside starting the link; the
// first valid RC packet ends it early
constexpr uint32_t BOOT_LISTEN_MS = 1500;
// the same after a BIND, long enough for someone to answer the TX's update offer
constexpr uint32_t BIND_LISTEN_MS = 60000;
constexpr uint8_t MAGIC0 = 'E', MAGIC1 = 'U';
constexpr uint16_t DATA_MAX = 232;
// Packets the TX keeps in flight for any RX. The RX acks cumulatively ("next offset I
// expect") and only ever writes in order, so a gap means go-back-N from the oldest
// unacked. An ESP32 RX queues 32 packets, so its TX may keep more in flight; the
// ESP8285 holds 12.
constexpr uint8_t WINDOW = 8;
// An ESP32 RX acks in-order progress every ACK_EVERY packets, or ACK_DELAY_US after
// the last one, and anything else (a gap, a duplicate, an error, the last byte) at
// once. So a repeat of the current ack means a packet went missing, and the TX goes
// back then rather than at its timeout. A TX keeping fewer than ACK_EVERY packets in
// flight still works, one ACK_DELAY_US per window slower.
constexpr uint8_t ACK_EVERY = 8;
constexpr uint32_t ACK_DELAY_US = 2000;
#if defined(PLATFORM_ESP32)
// DATA: the default 1 Mbps caps a 232 B packet stream at ~80 KB/s before acks
constexpr wifi_phy_rate_t ESPNOW_RATE = WIFI_PHY_RATE_12M;
// the RX's answers: short, so the most robust rate costs little. At 12 Mbps up to 1
// in 8 acks failed to arrive on the bench, and each loss stalled the transfer.
constexpr wifi_phy_rate_t ACK_RATE = WIFI_PHY_RATE_1M_L;
#endif

// HelloAck.flags
// The RX wants the file's bytes as they are on the card (.bin.gz), not inflated: the
// ESP8285 has no second app slot, the raw image doesn't fit next to the running one,
// and its bootloader inflates gzip images itself.
constexpr uint8_t FLAG_WANTS_GZIP = 0x01;
// The RX answered from bind mode: a good FINISH binds it to the HELLO's UID
constexpr uint8_t FLAG_BINDING = 0x02;

// where an RX in bind mode hears a TX: ESP-NOW broadcast
constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum Type : uint8_t
{
    HELLO = 1,
    HELLO_ACK,
    BEGIN,
    BEGIN_ACK,
    DATA,
    DATA_ACK,
    FINISH,
    FINISH_ACK,
    CANCEL_UPDATE,
    CANCEL_UPDATE_ACK,
    BIND,
    BIND_ACK,
};

// Cancel.reason: why the TX gave up, so the RX's log says the same as the handset
enum CancelReason : uint8_t
{
    CANCEL_USER = 0,     // the user cancelled on the handset
    CANCEL_NO_IMAGE,     // nothing on the card for this RX
    CANCEL_BAD_IMAGE,    // wrong format, or too big for this RX
    CANCEL_CARD_ERROR,   // reading the card failed
    CANCEL_TRANSFER,     // the transfer itself failed
    CANCEL_OTHER,
};

enum Status : uint8_t
{
    ST_OK = 0,
    ST_TOO_BIG,
    ST_NO_PARTITION,
    ST_WRITE_FAILED,
    ST_BAD_HASH,
    ST_NOT_STARTED,
    ST_ARMED,
};

struct __attribute__((packed)) Header
{
    uint8_t magic[2];
    uint8_t type;
    uint8_t seq;
};

struct __attribute__((packed)) Hello
{
    Header h;
    uint8_t proto;     // PROTO_VERSION; absent (0) from the first transmitters
    uint8_t uid[6];    // the TX's bind UID: what a receiver in bind mode binds to
    uint8_t flags;     // HELLO_*
};

// Hello.flags: broadcast to receivers in bind mode. Only those answer it; a receiver
// listening on its UID's MAC hears broadcasts too, and must ignore this one.
constexpr uint8_t HELLO_BIND = 0x01;
inline bool isBindHello(const uint8_t *p, int len)
{
    return len >= (int)sizeof(Hello) && (((const Hello *)p)->flags & HELLO_BIND);
}

struct __attribute__((packed)) HelloAck
{
    Header h;
    char key[16];      // e.g. "esp32-2400"
    char version[24];
    char commit[8];
    uint32_t maxImage;
    uint8_t flags;     // FLAG_*; absent (0) from older receivers
    uint8_t proto;     // PROTO_VERSION; absent (0) from older receivers
    uint8_t domain;    // DOMAIN_*: which regulatory build this is; absent (0) means FCC
    char name[16];     // the RX's device name (as in the Lua), for the handset to show
    uint8_t otaVersion; // OTA_VERSION_ID: a different one can't link to this TX
};

// HelloAck.domain. The 2.4 GHz regulatory domain is compiled in, not set in the
// trailer, so an EU receiver must only ever get the LBT image: "<key>-lbt" on the card.
constexpr uint8_t DOMAIN_FCC = 0;
constexpr uint8_t DOMAIN_LBT = 1;
inline uint8_t buildDomain()
{
#if defined(Regulatory_Domain_EU_CE_2400)
    return DOMAIN_LBT;
#else
    return DOMAIN_FCC;
#endif
}

struct __attribute__((packed)) Bind
{
    Header h;
    uint8_t uid[6];
};

struct __attribute__((packed)) Begin
{
    Header h;
    uint32_t size;
};

struct __attribute__((packed)) StatusOnly
{
    Header h;
    uint8_t status;
};

struct __attribute__((packed)) Data
{
    Header h;
    uint32_t offset;
    uint8_t data[DATA_MAX];
};

struct __attribute__((packed)) DataAck
{
    Header h;
    uint32_t next;
    uint8_t status;
};

struct __attribute__((packed)) Finish
{
    Header h;
    uint8_t sha256[32];
};

struct __attribute__((packed)) FinishAck
{
    Header h;
    uint8_t status;
    char text[40];
};

struct __attribute__((packed)) Cancel
{
    Header h;
    uint8_t reason;    // CancelReason
    char text[40];     // what the handset shows, NUL-terminated
};

inline void macFromUid(const uint8_t *uid, uint8_t *mac)
{
    mac[0] = 0x02; // unicast, locally administered
    mac[1] = 'E';
    memcpy(&mac[2], &uid[2], 4);
}

// What a bind stores: the link's four UID bytes, the rest zero, exactly as a bind over
// the link does, so it doesn't matter how a receiver was bound
inline void uidFromBind(const uint8_t *uid, uint8_t *out)
{
    out[0] = out[1] = 0;
    memcpy(&out[2], &uid[2], 4);
}

inline bool valid(const uint8_t *p, int len)
{
    return len >= (int)sizeof(Header) && p[0] == MAGIC0 && p[1] == MAGIC1;
}
}
