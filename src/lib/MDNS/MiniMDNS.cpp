#include "MiniMDNS.h"

#if defined(PLATFORM_ESP8266) || defined(UNIT_TEST)

#include <string.h>
#include <stdio.h>

#if defined(UNIT_TEST)
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#endif
#ifndef strlen_P
#define strlen_P strlen
#endif
#ifndef memcpy_P
#define memcpy_P memcpy
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef snprintf_P
#define snprintf_P snprintf
#endif
#else
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <lwip/udp.h>
#include <lwip/igmp.h>
#include <include/UdpContext.h>
#endif

#define MDNS_PORT 5353
#define TTL_SHORT 120
#define TTL_LONG 4500
#define NAME_MAX 80

enum : uint16_t { T_A = 1, T_PTR = 12, T_TXT = 16, T_SRV = 33, T_ANY = 255 };

// Names in wire format (length-prefixed labels); the NUL doubles as the root label
static const char LOCAL_LABELS[] PROGMEM = "\x05" "local";
static const char SVC_LABELS[] PROGMEM = "\x05" "_http" "\x04" "_tcp" "\x05" "local";
static const char SD_LABELS[] PROGMEM = "\x09" "_services" "\x07" "_dns-sd" "\x04" "_udp" "\x05" "local";

static inline uint8_t lower(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

MiniMDNS::MiniMDNS(const char *hostname, uint16_t port) : _host(hostname), _port(port)
{
    uint8_t mac[6];
#if defined(UNIT_TEST)
    memcpy(mac, "\xAA\xBB\xCC\x01\x02\x03", 6);
#else
    WiFi.macAddress(mac);
#endif
    int n = snprintf_P(_arena, ARENA, PSTR("%s_%02X%02X%02X%02X%02X%02X"), hostname, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _instance = _arena;
    _used = (n < 0 || n >= (int)ARENA) ? ARENA : n + 1;
}

MiniMDNS::~MiniMDNS()
{
    if (_ctx)
    {
        respond(R_ALL, true);
    }
    close();
}

void MiniMDNS::addTxt(const char *key, const char *value)
{
    if (_pairs < MAX_TXT && key && value)
    {
        _txt[2 * _pairs] = key;
        _txt[2 * _pairs + 1] = value;
        _pairs++;
    }
}

const char *MiniMDNS::store(const char *s)
{
    size_t n = strlen(s) + 1;
    if (_used + n > ARENA)
    {
        return nullptr;
    }
    char *p = _arena + _used;
    memcpy(p, s, n);
    _used += n;
    return p;
}

/*
 * Query side
 */

// Copies the name at pos into out in wire format, following compression
// pointers. Returns the position after the name in the packet, 0 on error.
uint16_t MiniMDNS::readName(uint16_t pos, uint8_t *out)
{
    const uint16_t len = inSize();
    uint16_t o = 0;
    uint16_t end = 0;
    uint8_t jumps = 0;
    while (true)
    {
        if (pos >= len)
        {
            return 0;
        }
        uint8_t l = rd(pos);
        if (l & 0xC0)
        {
            if ((l & 0xC0) != 0xC0 || pos + 1 >= len || ++jumps > 4)
            {
                return 0;
            }
            uint16_t ptr = ((l & 0x3F) << 8) | rd(pos + 1);
            if (ptr >= pos)
            {
                return 0;
            }
            if (!end)
            {
                end = pos + 2;
            }
            pos = ptr;
            continue;
        }
        if (o + 1 + l >= NAME_MAX || pos + 1 + l > len)
        {
            return 0;
        }
        out[o++] = l;
        pos++;
        if (l == 0)
        {
            break;
        }
        while (l--)
        {
            out[o++] = rd(pos++);
        }
    }
    return end ? end : pos;
}

// name is in wire format. dyn is an optional RAM string for the first label,
// rest is the wire format tail in flash; its NUL doubles as the root label.
bool MiniMDNS::nameIs(const uint8_t *name, const char *dyn, const char *rest)
{
    if (dyn)
    {
        uint8_t l = *name++;
        if (l != strlen(dyn))
        {
            return false;
        }
        for (uint8_t i = 0; i < l; i++)
        {
            if (lower(name[i]) != lower(dyn[i]))
            {
                return false;
            }
        }
        name += l;
    }
    size_t n = strlen_P(rest) + 1;
    for (size_t i = 0; i < n; i++)
    {
        if (lower(name[i]) != lower(pgm_read_byte(rest + i)))
        {
            return false;
        }
    }
    return true;
}

void MiniMDNS::handlePacket()
{
    const uint16_t len = inSize();
    if (len < 12 || (rd(2) & 0x80))
    {
        return; // too short, or a response
    }
    uint16_t questions = (rd(4) << 8) | rd(5);
    uint16_t pos = 12;
    uint8_t mask = 0;
    uint8_t name[NAME_MAX];
    while (questions--)
    {
        pos = readName(pos, name);
        if (pos == 0 || pos + 4 > len)
        {
            break;
        }
        const uint16_t qtype = (rd(pos) << 8) | rd(pos + 1);
        pos += 4;
        const bool any = qtype == T_ANY;
        if ((any || qtype == T_A) && nameIs(name, _host, LOCAL_LABELS))
        {
            mask |= R_A;
        }
        else if ((any || qtype == T_PTR) && nameIs(name, nullptr, SVC_LABELS))
        {
            mask |= R_PTR;
        }
        else if ((any || qtype == T_SRV || qtype == T_TXT) && nameIs(name, _instance, SVC_LABELS))
        {
            mask |= (qtype == T_SRV) ? R_SRV : (qtype == T_TXT) ? R_TXT : (R_SRV | R_TXT);
        }
        else if ((any || qtype == T_PTR) && nameIs(name, nullptr, SD_LABELS))
        {
            mask |= R_SD;
        }
    }
    if (mask)
    {
        respond(mask, false);
    }
}

/*
 * Response side
 */

static uint8_t bitCount(uint8_t v)
{
    uint8_t n = 0;
    for (; v; v >>= 1)
    {
        n += v & 1;
    }
    return n;
}

void MiniMDNS::respond(uint8_t mask, bool goodbye)
{
    // RFC 6763 12.1 / 12.2: a PTR answer carries SRV, TXT and A along, SRV carries A
    uint8_t extra = 0;
    if (mask & R_PTR)
    {
        extra |= R_SRV | R_TXT | R_A;
    }
    if (mask & R_SRV)
    {
        extra |= R_A;
    }
    extra &= ~mask;

    w16(0);      // ID
    w16(0x8400); // response, authoritative
    w16(0);      // QDCOUNT
    w16(bitCount(mask));
    w16(0);      // NSCOUNT
    w16(bitCount(extra));
    writeRecords(mask, goodbye);
    writeRecords(extra, goodbye);
    flush();
}

void MiniMDNS::writeRecords(uint8_t set, bool goodbye)
{
    const uint32_t ttlLong = goodbye ? 0 : TTL_LONG;
    const uint32_t ttlShort = goodbye ? 0 : TTL_SHORT;
    if (set & R_SD)
    {
        wName(nullptr, SD_LABELS);
        rrHead(T_PTR, false, ttlLong, nameLen(nullptr, SVC_LABELS));
        wName(nullptr, SVC_LABELS);
    }
    if (set & R_PTR)
    {
        wName(nullptr, SVC_LABELS);
        rrHead(T_PTR, false, ttlLong, nameLen(_instance, SVC_LABELS));
        wName(_instance, SVC_LABELS);
    }
    if (set & R_SRV)
    {
        wName(_instance, SVC_LABELS);
        rrHead(T_SRV, true, ttlShort, 6 + nameLen(_host, LOCAL_LABELS));
        w16(0); // priority
        w16(0); // weight
        w16(_port);
        wName(_host, LOCAL_LABELS);
    }
    if (set & R_TXT)
    {
        wName(_instance, SVC_LABELS);
        rrHead(T_TXT, true, ttlLong, txtLen());
        writeTxt();
    }
    if (set & R_A)
    {
        wName(_host, LOCAL_LABELS);
        rrHead(T_A, true, ttlShort, 4);
        uint32_t ip = ifIp(); // already in network byte order
        wr(&ip, 4);
    }
}

void MiniMDNS::rrHead(uint16_t type, bool unique, uint32_t ttl, uint16_t rdlen)
{
    w16(type);
    w16(unique ? 0x8001 : 0x0001); // IN, cache flush on the records only we own
    w32(ttl);
    w16(rdlen);
}

uint16_t MiniMDNS::nameLen(const char *dyn, const char *rest)
{
    return (dyn ? 1 + strlen(dyn) : 0) + strlen_P(rest) + 1;
}

void MiniMDNS::wName(const char *dyn, const char *rest)
{
    if (dyn)
    {
        uint8_t l = strlen(dyn);
        wr(&l, 1);
        wr(dyn, l);
    }
    wP(rest, strlen_P(rest) + 1);
}

// Each TXT item is "key=value" behind a length byte, capped at 255
uint16_t MiniMDNS::txtLen()
{
    uint16_t n = 0;
    for (uint8_t i = 0; i < _pairs; i++)
    {
        size_t l = strlen_P(_txt[2 * i]) + 1 + strlen_P(_txt[2 * i + 1]);
        n += 1 + (l > 255 ? 255 : l);
    }
    return n;
}

void MiniMDNS::writeTxt()
{
    for (uint8_t i = 0; i < _pairs; i++)
    {
        const char *key = _txt[2 * i];
        const char *value = _txt[2 * i + 1];
        size_t kl = strlen_P(key);
        size_t vl = strlen_P(value);
        size_t l = kl + 1 + vl;
        if (l > 255)
        {
            vl -= l - 255;
            l = 255;
        }
        uint8_t lb = l;
        wr(&lb, 1);
        wP(key, kl);
        wr("=", 1);
        wP(value, vl);
    }
}

void MiniMDNS::w16(uint16_t v)
{
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
    wr(b, 2);
}

void MiniMDNS::w32(uint32_t v)
{
    uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    wr(b, 4);
}

// Copy n bytes from a string that may be in flash
void MiniMDNS::wP(const char *p, size_t n)
{
    char buf[32];
    while (n)
    {
        size_t c = n > sizeof(buf) ? sizeof(buf) : n;
        memcpy_P(buf, p, c);
        wr(buf, c);
        p += c;
        n -= c;
    }
}

/*
 * Platform side
 */

#if defined(UNIT_TEST)

static const uint8_t *tIn;
static size_t tInLen;
static uint8_t *tOut;
static size_t tOutLen;
static size_t tOutMax;

size_t MiniMDNS::testHandle(const uint8_t *in, size_t len, uint8_t *out, size_t max)
{
    tIn = in;
    tInLen = len;
    tOut = out;
    tOutLen = 0;
    tOutMax = max;
    handlePacket();
    return tOutLen;
}

size_t MiniMDNS::testAnnounce(uint8_t *out, size_t max, bool goodbye)
{
    tOut = out;
    tOutLen = 0;
    tOutMax = max;
    respond(R_ALL, goodbye);
    return tOutLen;
}

uint16_t MiniMDNS::inSize() { return tInLen; }
uint8_t MiniMDNS::rd(uint16_t pos) { return tIn[pos]; }
void MiniMDNS::wr(const void *p, size_t n)
{
    if (tOutLen + n <= tOutMax)
    {
        memcpy(tOut + tOutLen, p, n);
    }
    tOutLen += n;
}
void MiniMDNS::flush() {}
uint32_t MiniMDNS::ifIp() { return 10 | (1u << 24); } // 10.0.0.1 in memory order
void MiniMDNS::open() {}
void MiniMDNS::close() {}
void MiniMDNS::update() {}

#else

static IPAddress mdnsGroup()
{
    return IPAddress(224, 0, 0, 251);
}

void MiniMDNS::open()
{
    IPAddress ifa(_ip);
    if (igmp_joingroup(ifa, mdnsGroup()) != ERR_OK)
    {
        return;
    }
    _ctx = new UdpContext;
    _ctx->ref();
    _ctx->setMulticastInterface(ifa);
    _ctx->setMulticastTTL(255);
    if (!_ctx->listen(IPAddress(), MDNS_PORT))
    {
        close();
    }
}

void MiniMDNS::close()
{
    if (_ctx)
    {
        _ctx->unref();
        _ctx = nullptr;
    }
    if (_ip)
    {
        igmp_leavegroup(IPAddress(_ip), mdnsGroup());
    }
}

void MiniMDNS::update()
{
    const uint32_t ip = (WiFi.getMode() & WIFI_AP) ? (uint32_t)WiFi.softAPIP() : (uint32_t)WiFi.localIP();
    if (ip != _ip)
    {
        close();
        _ip = ip;
        if (ip)
        {
            open();
        }
        if (_ctx)
        {
            // RFC 6762 8.3: announce at least twice, a second apart
            respond(R_ALL, false);
            _announceAt = millis() + 1000;
        }
    }
    if (!_ctx)
    {
        return;
    }
    if (_announceAt && (int32_t)(millis() - _announceAt) >= 0)
    {
        _announceAt = 0;
        respond(R_ALL, false);
    }
    while (_ctx->next())
    {
        handlePacket();
    }
}

uint16_t MiniMDNS::inSize() { return _ctx->tell() + _ctx->getSize(); }
uint8_t MiniMDNS::rd(uint16_t pos)
{
    _ctx->seek(pos);
    return _ctx->read();
}
void MiniMDNS::wr(const void *p, size_t n) { _ctx->append(reinterpret_cast<const char *>(p), n); }
void MiniMDNS::flush() { _ctx->send(mdnsGroup(), MDNS_PORT); }
uint32_t MiniMDNS::ifIp() { return _ip; }

#endif

#endif // PLATFORM_ESP8266 || UNIT_TEST
