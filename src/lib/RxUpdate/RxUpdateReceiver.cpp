#if defined(TARGET_RX) && defined(PLATFORM_ESP32)

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

#include <WiFi.h>
#include <esp_now.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>

using namespace RxUpdate;

// The TX gives up well before this; it's only so a dead session doesn't strand the RX
static constexpr uint32_t IDLE_REBOOT_MS = 30000;
static constexpr size_t TRAILER_SIZE =
    ELRSOPTS_PRODUCTNAME_SIZE + ELRSOPTS_DEVICENAME_SIZE + ELRSOPTS_OPTIONS_SIZE + ELRSOPTS_HARDWARE_SIZE;

#if defined(PLATFORM_ESP32_S3)
#define KEY_PLATFORM "esp32s3"
#elif defined(PLATFORM_ESP32_C3)
#define KEY_PLATFORM "esp32c3"
#else
#define KEY_PLATFORM "esp32"
#endif
#if defined(RADIO_SX128X)
#define KEY_RADIO "2400"
#elif defined(RADIO_LR1121)
#define KEY_RADIO "lr1121"
#elif defined(RADIO_LR2021)
#define KEY_RADIO "lr2021"
#else
#define KEY_RADIO "900"
#endif

struct Packet
{
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[250];
};

static QueueHandle_t rxQueue;
static uint8_t peer[6];
static bool havePeer = false;
static uint32_t lastPacket = 0;
static uint32_t expected = 0, imageSize = 0;
// esp_ota_write() per 232 B packet ran ~1.2 ms each (a flash program per call, most of
// them straddling two pages) and capped the transfer at ~190 KB/s. Whole pages are
// cheaper per byte, but not too many at once: a 4 KB write keeps the cache off for
// ~13 ms, long enough for the radio to drop frames
static uint8_t wbuf[1024];
static uint16_t wLen = 0;
// a DATA_ACK held back (ACK_EVERY), and when it's due
static uint8_t unacked = 0;
static DataAck pendingAck;
static uint32_t ackDue = 0;
static bool started = false;
static esp_ota_handle_t ota = 0;
static const esp_partition_t *target = nullptr;
static mbedtls_sha256_context sha;
static uint32_t rebootAt = 0;
static uint32_t packets = 0, lastReport = 0;
static bool finished = false;
static FinishAck finishResult;
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

// Update mode owns UART0 too (on some RXes those pins are PWM outputs): use it as a
// plain log, it's the only window into this mode without a debug build
#define RXU_LOG(...) Serial.printf("RXU " __VA_ARGS__)

static void onRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    if (!valid(data, len) || len > 250)
        return;
    Packet p;
    memcpy(p.mac, mac, 6);
    p.len = len;
    memcpy(p.data, data, len);
    xQueueSend(rxQueue, &p, 0);
}

static void send(const void *pkt, size_t len)
{
    esp_now_send(peer, (const uint8_t *)pkt, len);
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
    // the TX keeps up to RxUpdate::WINDOW packets in flight; leave room for a burst
    if (!rxQueue)
        rxQueue = xQueueCreate(32, sizeof(Packet));
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_get_mac(WIFI_IF_STA, originalMac);
    if (!ownMac)
    {
        uint8_t mac[6];
        macFromUid(UID, mac);
        esp_wifi_set_mac(WIFI_IF_STA, mac);
    }
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    esp_now_init();
    esp_wifi_config_espnow_rate(WIFI_IF_STA, ACK_RATE);
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
    esp_wifi_set_mac(WIFI_IF_STA, originalMac);
    WiFi.mode(WIFI_OFF);
    espNowUp = false;
    havePeer = false;
    xQueueReset(rxQueue);
}

static void enterUpdateMode()
{
    DBGLN("Entering RX update mode");
    setConnectionState(rxUpdate);
    hwTimer::stop();
    Radio.End();

    Serial.end();
    Serial.begin(115200, SERIAL_8N1, 3, 1);

    startEspNow();
    uint8_t got[6];
    esp_wifi_get_mac(WIFI_IF_STA, got);
    uint8_t ch;
    wifi_second_chan_t ch2;
    esp_wifi_get_channel(&ch, &ch2);
    RXU_LOG("enter: mac %02x:%02x:%02x:%02x:%02x:%02x ch %u\n", got[0], got[1], got[2], got[3], got[4], got[5], ch);
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
        memcpy(UID, b->uid, UID_LEN);
        config.SetUID(UID);
        boundOverEspNow = true;
    }
    send(&a, sizeof(a));
}

static void bindListenLoop()
{
    Packet p;
    while (xQueueReceive(rxQueue, &p, 0) == pdTRUE)
    {
        const auto h = (const Header *)p.data;
        const auto hello = (const Hello *)p.data;
        // a TX offering to bind us: say who we are, and stay in bind mode until it
        // actually starts. It repeats HELLO until it hears back; the newest one wins.
        if (h->type == HELLO && isBindHello(p.data, p.len) && OtaUidIsBound(hello->uid))
        {
            memcpy(bindPeer, p.mac, 6);
            memcpy(bindUid, hello->uid, 6);
            bindAnswered = true;
            sendHelloAck(p);
        }
        else if (h->type == BIND && bindAnswered && memcmp(p.mac, bindPeer, 6) == 0 && p.len >= sizeof(Bind))
        {
            takeBind(p);
        }
        else if (h->type == BEGIN && bindAnswered && memcmp(p.mac, bindPeer, 6) == 0)
        {
            bindListening = false;
            binding = true;
            enterUpdateMode();
            RXU_LOG("bind mode: updating, then binding to the TX that found us\n");
            xQueueSend(rxQueue, &p, 0);
            return;
        }
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
    Packet p, hello;
    bool found = false;
    while (xQueueReceive(rxQueue, &p, 0) == pdTRUE)
    {
        if (((const Header *)p.data)->type == HELLO && !isBindHello(p.data, p.len))
        {
            hello = p;
            found = true;
        }
    }
    if (found)
    {
        bootListening = false;
        enterUpdateMode();
        RXU_LOG("TX found us while listening\n");
        xQueueSend(rxQueue, &hello, 0);
    }
}

static void addPeer(const uint8_t *mac)
{
    if (havePeer && memcmp(peer, mac, 6) == 0)
        return;
    if (havePeer)
        esp_now_del_peer(peer);
    memcpy(peer, mac, 6);
    esp_now_peer_info_t info = {};
    memcpy(info.peer_addr, mac, 6);
    info.channel = ESPNOW_CHANNEL;
    info.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&info);
    havePeer = true;
}

// Buffer in-order image bytes, writing whole wbufs to flash
static bool writeImage(const uint8_t *data, size_t n)
{
    while (n)
    {
        const size_t take = std::min(n, sizeof(wbuf) - wLen);
        memcpy(&wbuf[wLen], data, take);
        wLen += take;
        data += take;
        n -= take;
        if (wLen == sizeof(wbuf))
        {
            wLen = 0;
            if (esp_ota_write(ota, wbuf, sizeof(wbuf)) != ESP_OK)
                return false;
        }
    }
    return true;
}

static bool flushImage()
{
    const uint16_t n = wLen;
    wLen = 0;
    return !n || esp_ota_write(ota, wbuf, n) == ESP_OK;
}

static uint8_t finish(const Finish *f, char *text, size_t textLen)
{
    if (!started || expected != imageSize)
    {
        snprintf(text, textLen, "short image %u/%u", expected, imageSize);
        return ST_NOT_STARTED;
    }
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    if (memcmp(digest, f->sha256, 32) != 0)
    {
        esp_ota_abort(ota);
        started = false;
        snprintf(text, textLen, "hash mismatch");
        return ST_BAD_HASH;
    }

    // Carry our own trailer (product name, options, hardware layout) across, so the
    // new image boots with this RX's layout and bind phrase
    static uint8_t trailer[TRAILER_SIZE];
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!flushImage() || esp_partition_read(running, ESP.getSketchSize(), trailer, TRAILER_SIZE) != ESP_OK ||
        esp_ota_write(ota, trailer, TRAILER_SIZE) != ESP_OK)
    {
        esp_ota_abort(ota);
        started = false;
        snprintf(text, textLen, "trailer copy failed");
        return ST_WRITE_FAILED;
    }
    // esp_ota_end() verifies the image (segments + appended SHA-256) before we point the
    // bootloader at it
    esp_err_t err = esp_ota_end(ota);
    if (err == ESP_OK)
        err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK)
    {
        started = false;
        snprintf(text, textLen, "verify failed (%d)", err);
        return ST_WRITE_FAILED;
    }
    started = false;
    if (binding)
    {
        // exactly what a bind over the link stores, with the TX's whole UID
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
    strncpy(a.key, KEY_PLATFORM "-" KEY_RADIO, sizeof(a.key));
    strncpy(a.version, version, sizeof(a.version));
    strncpy(a.commit, commit, sizeof(a.commit));
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    a.maxImage = next ? next->size - TRAILER_SIZE : 0;
    a.flags = bindListening || binding ? FLAG_BINDING : 0;
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
            // a retried BEGIN after we already started: same answer
            a.status = imageSize == b->size ? ST_OK : ST_WRITE_FAILED;
        }
        // esp_ota_begin() erases the whole range now (a couple of seconds) so the
        // stream itself never stalls on a sector erase; the TX waits it out
        else if (!(target = esp_ota_get_next_update_partition(nullptr)) || b->size + TRAILER_SIZE > target->size ||
                 esp_ota_begin(target, b->size + TRAILER_SIZE, &ota) != ESP_OK)
        {
            a.status = ST_TOO_BIG;
        }
        else
        {
            started = true;
            imageSize = b->size;
            expected = 0;
            unacked = 0;
            wLen = 0;
            mbedtls_sha256_init(&sha);
            mbedtls_sha256_starts_ret(&sha, 0);
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
            // only ever in order: a duplicate (lost ACK) or a gap gets the same
            // "next expected" answer and the TX resends from there
            if (!writeImage(d->data, n))
            {
                a.status = ST_WRITE_FAILED;
            }
            else
            {
                mbedtls_sha256_update_ret(&sha, d->data, n);
                expected += n;
            }
        }
        a.next = expected;
        // An ack at ACK_RATE costs more air than the DATA it answers, so in-order
        // progress is acked every ACK_EVERY packets, or ACK_DELAY_US after the last
        // one. Anything else (a gap, a duplicate, an error, the last byte) right away:
        // the TX goes back on a repeated ack.
        if (a.status == ST_OK && d->offset + n == expected && expected < imageSize && ++unacked < ACK_EVERY)
        {
            pendingAck = a;
            ackDue = micros() + ACK_DELAY_US;
            break;
        }
        unacked = 0;
        send(&a, sizeof(a));
        break;
    }

    case FINISH:
    {
        // esp_ota_end() reads the whole image back to verify it, which can outlast the
        // TX's patience; a retried FINISH must get the same answer, not "not started"
        if (!finished)
        {
            finishResult = {};
            finishResult.status = finish((const Finish *)p.data, finishResult.text, sizeof(finishResult.text));
            finished = finishResult.status == ST_OK;
            // stay up long enough to answer a FINISH whose answer got lost
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
        // A finished update already points the bootloader at the new image and stays
        // that way; anything less goes, and we come back on the image we have
        if (started)
            esp_ota_abort(ota);
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
    Packet p;
    while (xQueueReceive(rxQueue, &p, 0) == pdTRUE)
    {
        lastPacket = now;
        packets++;
        handle(p);
    }
    if (unacked && (int32_t)(micros() - ackDue) >= 0)
    {
        unacked = 0;
        send(&pendingAck, sizeof(pendingAck));
    }
    if ((int32_t)(now - lastReport) >= 2000)
    {
        lastReport = now;
        RXU_LOG("alive: %u packets, expected %u/%u\n", packets, expected, imageSize);
    }
    // signed: `now` is sampled before enter() runs from the deferred queue, so it can
    // be a few ms older than lastPacket on the first pass
    if ((rebootAt && (int32_t)(now - rebootAt) > 0) || (int32_t)(now - lastPacket) > (int32_t)IDLE_REBOOT_MS)
    {
        if (started)
            esp_ota_abort(ota);
        ESP.restart();
    }
}

#endif
