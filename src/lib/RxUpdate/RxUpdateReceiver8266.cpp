#if defined(TARGET_RX) && defined(PLATFORM_ESP8266)

// ESP8285 side of the TX -> RX firmware transfer (see RxUpdateProto.h).
//
// Differences from the ESP32 receiver:
//  * There's no second app slot. The updater writes into the free space after the
//    running sketch and eboot copies it over on reboot, so the image has to fit in
//    ~430 KB: the RX asks for the .bin.gz as-is (FLAG_WANTS_GZIP) and eboot inflates it.
//  * Our own options/hardware trailer can't be appended (it would have to be inside the
//    gzip stream), so the image ends in an erased one and ours waits in LittleFS
//    (/trailer.bin) for the new firmware to write back on its first boot. After that
//    the RX is exactly as a web update would have left it.

#include "RxUpdateReceiver.h"
#include "RxUpdateProto.h"

#include <Arduino.h>
#include "targets.h"
#include "device.h"
#include "common.h"
#include "OTA.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <bearssl/bearssl_hash.h>
#include <espnow.h>
extern "C" {
#include <user_interface.h>
}

using namespace RxUpdate;

static constexpr uint32_t IDLE_REBOOT_MS = 30000;

#if defined(RADIO_SX128X)
#define KEY_RADIO "2400"
#else
#define KEY_RADIO "900"
#endif

#define RXU_LOG(...) Serial.printf("RXU " __VA_ARGS__)

// The recv callback runs in the SDK's context: copy into a small ring, handle in loop()
struct Packet
{
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[250];
};
static constexpr uint8_t RING = 12; // > RxUpdate::WINDOW, and RAM is tight on a 8285
static Packet ring[RING];
static volatile uint8_t ringHead = 0, ringTail = 0;

static uint8_t peer[6];
static bool havePeer = false;
static uint32_t lastPacket = 0, lastReport = 0, packets = 0;
static uint32_t expected = 0, imageSize = 0;
static bool started = false;
static bool finished = false;
static FinishAck finishResult;
static br_sha256_context sha;
static uint32_t rebootAt = 0;
static bool espNowUp = false;
static uint8_t originalMac[6];
static bool bootListening = false;
static uint32_t bootListenEnd = 0;
static uint32_t bootListenPacket = 0; // LastValidPacket when it started: a newer one ends it
static bool boundOverEspNow = false;  // a BIND came in: listen on the new UID's MAC next
// bind mode: the TX we answered, and the UID to bind to once its image is in
static bool bindListening = false;
static bool bindAnswered = false;
static bool binding = false;
static uint8_t bindPeer[6];
static uint8_t bindUid[6];

extern uint32_t LastValidPacket;

static void onRecv(uint8_t *mac, uint8_t *data, uint8_t len)
{
    if (!valid(data, len))
        return;
    const uint8_t next = (ringHead + 1) % RING;
    if (next == ringTail)
        return; // full: the TX's go-back-N resends it
    Packet &p = ring[ringHead];
    memcpy(p.mac, mac, 6);
    p.len = len;
    memcpy(p.data, data, len);
    ringHead = next;
}

static void send(const void *pkt, size_t len)
{
    esp_now_send(peer, (uint8_t *)pkt, len);
}

static void header(Header &h, Type t, uint8_t seq)
{
    h.magic[0] = MAGIC0;
    h.magic[1] = MAGIC1;
    h.type = t;
    h.seq = seq;
}

// Station on ESPNOW_CHANNEL with the MAC derived from the bind UID, so the TX can
// address us without any discovery. In bind mode we have no UID worth the name, so
// keep our own MAC and hear the TX's broadcast instead.
static void startEspNow(bool ownMac = false)
{
    if (espNowUp)
        return;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    wifi_get_macaddr(STATION_IF, originalMac);
    if (!ownMac)
    {
        uint8_t mac[6];
        macFromUid(UID, mac);
        wifi_set_macaddr(STATION_IF, mac);
    }
    wifi_set_channel(ESPNOW_CHANNEL);
    esp_now_init();
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(onRecv);
    espNowUp = true;
}

// Back to how devWIFI left it: off, and with our own MAC should it start WiFi later
static void stopEspNow()
{
    if (!espNowUp)
        return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    wifi_set_macaddr(STATION_IF, originalMac);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    espNowUp = false;
    havePeer = false;
    ringHead = ringTail = 0;
}

static void enterUpdateMode()
{
    setConnectionState(rxUpdate);
    hwTimer::stop();
    Radio.End();

    // Update mode owns UART0 (normally the CRSF output): use it as a plain log
    Serial.end();
    Serial.begin(115200);

    startEspNow();
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    RXU_LOG("enter: mac %02x:%02x:%02x:%02x:%02x:%02x ch %u free %u\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            wifi_get_channel(), ESP.getFreeSketchSpace());
    lastPacket = millis();
}

bool RxUpdateReceiver::enter()
{
    if (CRSF_to_BIT(ChannelData[4]))
    {
        DBGLN("RX update refused: armed");
        return false;
    }
    enterUpdateMode();
    return true;
}

static void listenFor(uint32_t ms)
{
    startEspNow();
    bootListening = true;
    bootListenEnd = millis() + ms;
    bootListenPacket = LastValidPacket;
}

void RxUpdateReceiver::bootListen()
{
    listenFor(BOOT_LISTEN_MS);
}

void RxUpdateReceiver::bindListen(bool on)
{
    if (connectionState == rxUpdate)
        return;
    if (on)
    {
        // a boot listen (on our UID's MAC) gives way: in bind mode we answer as ourselves
        bootListening = false;
        stopEspNow();
        startEspNow(true);
        bindListening = true;
        bindAnswered = false;
    }
    else if (bindListening)
    {
        bindListening = false;
        stopEspNow();
        // bound over ESP-NOW: the TX may offer an update next, and we may not be able
        // to link to it (another major version), so listen on our new UID's MAC
        if (boundOverEspNow)
        {
            boundOverEspNow = false;
            listenFor(BIND_LISTEN_MS);
        }
    }
}

void RxUpdateReceiver::stopListening()
{
    if (connectionState == rxUpdate)
        return;
    bootListening = bindListening = false;
    stopEspNow();
}

static void sendHelloAck(const Packet &p);

// The TX's Bind found us: take its UID exactly as a bind over the link would. Changing
// the config is what ends bind mode (rx_main), and that commits it.
static void takeBind(const Packet &p)
{
    const auto b = (const Bind *)p.data;
    StatusOnly a;
    header(a.h, BIND_ACK, b->h.seq);
    a.status = ST_OK;
    if (OtaUidIsBound(b->uid))
    {
        uidFromBind(b->uid, UID);
        config.SetUID(UID);
        boundOverEspNow = true;
    }
    send(&a, sizeof(a));
}

static void bindListenLoop()
{
    while (ringTail != ringHead)
    {
        const Packet &p = ring[ringTail];
        const auto h = (const Header *)p.data;
        const auto hello = (const Hello *)p.data;
        // a TX offering to bind us: say who we are, and stay in bind mode until it
        // actually starts. It repeats HELLO until it hears back.
        if (h->type == HELLO && isBindHello(p.data, p.len) && OtaUidIsBound(hello->uid))
        {
            memcpy(bindPeer, p.mac, 6);
            uidFromBind(hello->uid, bindUid);
            bindAnswered = true;
            sendHelloAck(p);
        }
        else if (h->type == BIND && bindAnswered && memcmp(p.mac, bindPeer, 6) == 0 && p.len >= sizeof(Bind))
        {
            takeBind(p);
        }
        else if (h->type == BEGIN && bindAnswered && memcmp(p.mac, bindPeer, 6) == 0)
        {
            // leave the BEGIN in the ring for loop() to answer
            bindListening = false;
            binding = true;
            enterUpdateMode();
            RXU_LOG("bind mode: updating, then binding to the TX that found us\n");
            return;
        }
        ringTail = (ringTail + 1) % RING;
    }
}

void RxUpdateReceiver::listenLoop(uint32_t now)
{
    if (bindListening)
    {
        bindListenLoop();
        return;
    }
    if (!bootListening)
        return;
    // A TX is talking to us over the link: that wins, WiFi goes back off. (A TX
    // looking for us over ESP-NOW has its radio off, so the two can't both happen.)
    if (LastValidPacket != bootListenPacket || (int32_t)(now - bootListenEnd) > 0)
    {
        bootListening = false;
        stopEspNow();
        return;
    }
    // A HELLO for us: become the update receiver, loop() answers it. The TX repeats
    // HELLO until it hears back, so only the newest one is worth keeping.
    for (uint8_t i = ringTail; i != ringHead; i = (i + 1) % RING)
    {
        if (((const Header *)ring[i].data)->type == HELLO && !isBindHello(ring[i].data, ring[i].len))
        {
            bootListening = false;
            ringTail = i;
            enterUpdateMode();
            RXU_LOG("TX found us while listening\n");
            return;
        }
    }
}

static void addPeer(const uint8_t *mac)
{
    if (havePeer && memcmp(peer, mac, 6) == 0)
        return;
    if (havePeer)
        esp_now_del_peer(peer);
    memcpy(peer, mac, 6);
    esp_now_add_peer(peer, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, nullptr, 0);
    havePeer = true;
}

// The new image arrives with an erased trailer: leave a copy of ours in LittleFS for it to
// write back on first boot (options_init). Whatever the user changed in the web UI
// (/options.json, /hardware.json) stays where it is, exactly as with a web update.
static bool stashTrailer()
{
    uint32_t buf[64];
    bool blank = true;
    File f = LittleFS.open(ELRSOPTS_TRAILER_STASH, "w");
    if (!f)
        return false;
    for (size_t off = 0; off < ELRSOPTS_TRAILER_SIZE; off += sizeof(buf))
    {
        const size_t n = std::min(sizeof(buf), ELRSOPTS_TRAILER_SIZE - off);
        if (!ESP.flashRead(ESP.getSketchSize() + off, buf, n) || f.write((uint8_t *)buf, n) != n)
            return false;
        for (size_t i = 0; i < n / 4; i++)
            blank &= buf[i] == 0xFFFFFFFF;
    }
    f.close();
    // nothing to carry (a bare image): don't make the new one write 0xFF over 0xFF
    if (blank)
        LittleFS.remove(ELRSOPTS_TRAILER_STASH);
    return true;
}

static uint8_t finish(const Finish *f, char *text, size_t textLen)
{
    if (!started || expected != imageSize)
    {
        snprintf(text, textLen, "short image %u/%u", expected, imageSize);
        return ST_NOT_STARTED;
    }
    uint8_t digest[32];
    br_sha256_out(&sha, digest);
    if (memcmp(digest, f->sha256, 32) != 0)
    {
        // no end(): on the ESP8266 that would schedule eboot to copy this image
        started = false;
        snprintf(text, textLen, "hash mismatch");
        return ST_BAD_HASH;
    }
    if (!stashTrailer())
    {
        // no end(): on the ESP8266 that would schedule eboot to copy this image
        started = false;
        snprintf(text, textLen, "config carry failed");
        return ST_WRITE_FAILED;
    }
    // end() checks the size matches what begin() was given and tells eboot to copy
    if (!Update.end())
    {
        started = false;
        snprintf(text, textLen, "update failed (%u)", Update.getError());
        return ST_WRITE_FAILED;
    }
    started = false;
    if (binding)
    {
        // exactly what a bind over the link stores
        config.SetUID(bindUid);
        config.Commit();
        RXU_LOG("bound to %u,%u,%u,%u,%u,%u\n", bindUid[0], bindUid[1], bindUid[2], bindUid[3], bindUid[4],
                bindUid[5]);
    }
    snprintf(text, textLen, binding ? "ok, bound, rebooting" : "ok, rebooting");
    return ST_OK;
}

static void sendHelloAck(const Packet &p)
{
    addPeer(p.mac);
    HelloAck a = {};
    header(a.h, HELLO_ACK, ((const Header *)p.data)->seq);
    strncpy(a.key, "esp8285-" KEY_RADIO, sizeof(a.key));
    strncpy(a.version, version, sizeof(a.version));
    strncpy(a.commit, commit, sizeof(a.commit));
    // same margin the web updater uses
    a.maxImage = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    a.flags = FLAG_WANTS_GZIP | (bindListening || binding ? FLAG_BINDING : 0);
    a.proto = PROTO_VERSION;
    a.domain = buildDomain();
    strncpy(a.name, device_name, sizeof(a.name));
    a.otaVersion = OTA_VERSION_ID;
    send(&a, sizeof(a));
}

static void handle(const Packet &p)
{
    const auto h = (const Header *)p.data;
    addPeer(p.mac);

    switch (h->type)
    {
    case HELLO:
        // a Bind Receiver HELLO is for receivers in bind mode, unless that's how we got here
        if (!isBindHello(p.data, p.len) || binding)
            sendHelloAck(p);
        break;

    case BEGIN:
    {
        const auto b = (const Begin *)p.data;
        StatusOnly a;
        header(a.h, BEGIN_ACK, h->seq);
        if (started)
        {
            a.status = imageSize == b->size ? ST_OK : ST_WRITE_FAILED;
        }
        else if (!Update.begin(b->size, U_FLASH))
        {
            a.status = ST_TOO_BIG;
        }
        else
        {
            started = true;
            imageSize = b->size;
            expected = 0;
            br_sha256_init(&sha);
            a.status = ST_OK;
        }
        send(&a, sizeof(a));
        break;
    }

    case DATA:
    {
        const auto d = (const Data *)p.data;
        const int n = p.len - (int)offsetof(Data, data);
        DataAck a;
        header(a.h, DATA_ACK, h->seq);
        a.status = ST_OK;
        if (!started)
        {
            a.status = ST_NOT_STARTED;
        }
        else if (d->offset == expected && n > 0 && expected + n <= imageSize)
        {
            // in order only; the ESP8266 updater erases sector by sector as it goes,
            // and the TX's window rides out those stalls
            if (Update.write((uint8_t *)d->data, n) != (size_t)n)
            {
                a.status = ST_WRITE_FAILED;
            }
            else
            {
                br_sha256_update(&sha, d->data, n);
                expected += n;
            }
        }
        a.next = expected;
        send(&a, sizeof(a));
        break;
    }

    case FINISH:
    {
        if (!finished)
        {
            finishResult = {};
            finishResult.status = finish((const Finish *)p.data, finishResult.text, sizeof(finishResult.text));
            finished = finishResult.status == ST_OK;
            if (finished)
                rebootAt = millis() + 5000;
        }
        header(finishResult.h, FINISH_ACK, h->seq);
        send(&finishResult, sizeof(finishResult));
        break;
    }

    case CANCEL_UPDATE:
    {
        const auto c = (const Cancel *)p.data;
        char why[sizeof(c->text) + 1] = {};
        if (p.len > offsetof(Cancel, text))
            memcpy(why, c->text, std::min<size_t>(sizeof(c->text), p.len - offsetof(Cancel, text)));
        RXU_LOG("cancelled by TX (%u): %s\n", p.len > offsetof(Cancel, reason) ? c->reason : CANCEL_OTHER, why);
        StatusOnly a;
        header(a.h, CANCEL_UPDATE_ACK, h->seq);
        a.status = ST_OK;
        send(&a, sizeof(a));
        // Never Update.end() here, that would have eboot copy a partial image. A
        // finished update is already scheduled and stays that way.
        started = false;
        // a moment to answer a repeated CANCEL_UPDATE whose ACK got lost
        if (!rebootAt)
            rebootAt = millis() + 1000;
        break;
    }
    }
}

void RxUpdateReceiver::loop(uint32_t now)
{
    while (ringTail != ringHead)
    {
        lastPacket = now;
        packets++;
        handle(ring[ringTail]);
        ringTail = (ringTail + 1) % RING;
    }
    if ((int32_t)(now - lastReport) >= 2000)
    {
        lastReport = now;
        RXU_LOG("alive: %u packets, expected %u/%u\n", packets, expected, imageSize);
    }
    if ((rebootAt && (int32_t)(now - rebootAt) > 0) || (int32_t)(now - lastPacket) > (int32_t)IDLE_REBOOT_MS)
    {
        // after a good FINISH, end() already told eboot to copy (and inflate) the new
        // image; otherwise nothing was scheduled and we come back on the old one
        ESP.restart();
    }
}

#endif
