#pragma once

#include <stdint.h>
#include <stddef.h>

class UdpContext;

/**
 * Minimal mDNS / DNS-SD responder for the ESP8266.
 *
 * One host name, one service instance, a few TXT records. It answers A, PTR,
 * SRV and TXT queries for its own names plus the DNS-SD service enumeration,
 * announces itself when an interface comes up and says goodbye when deleted.
 * There is no probing, no query support, no known-answer suppression and no
 * IPv6, which is what keeps it at a fraction of the size of LEAmDNS.
 *
 * Everything the responder needs lives in the object, so the static cost is
 * the pointer that owns it and the heap is only used while WiFi is up.
 */
class MiniMDNS
{
public:
    static constexpr uint8_t MAX_TXT = 8;
    static constexpr size_t ARENA = 128;

    // hostname is not copied and must outlive the object. The service
    // instance name is <hostname>_<MAC> so devices can share a hostname.
    MiniMDNS(const char *hostname, uint16_t port);
    ~MiniMDNS();

    // key and value may be in RAM or flash, neither is copied.
    void addTxt(const char *key, const char *value);
    // Copies s into the object; use it for a value built from a temporary.
    const char *store(const char *s);
    // Call from the main loop. Binds when an interface has an address,
    // rebinds when it changes, answers queries.
    void update();

#if defined(UNIT_TEST)
    size_t testHandle(const uint8_t *in, size_t len, uint8_t *out, size_t max);
    size_t testAnnounce(uint8_t *out, size_t max, bool goodbye);
    const char *instance() const { return _instance; }
#endif

private:
    enum : uint8_t { R_A = 1, R_PTR = 2, R_SRV = 4, R_TXT = 8, R_SD = 16, R_ALL = 31 };

    void open();
    void close();
    void handlePacket();
    void respond(uint8_t mask, bool goodbye);
    void writeRecords(uint8_t set, bool goodbye);
    void rrHead(uint16_t type, bool unique, uint32_t ttl, uint16_t rdlen);
    uint16_t readName(uint16_t pos, uint8_t *out);
    bool nameIs(const uint8_t *name, const char *dyn, const char *rest);
    void wName(const char *dyn, const char *rest);
    uint16_t nameLen(const char *dyn, const char *rest);
    uint16_t txtLen();
    void writeTxt();

    // packet I/O, one implementation per platform
    uint16_t inSize();
    uint8_t rd(uint16_t pos);
    void wr(const void *p, size_t n);
    void wP(const char *p, size_t n);
    void w16(uint16_t v);
    void w32(uint32_t v);
    void flush();
    uint32_t ifIp();

    UdpContext *_ctx = nullptr;
    const char *_host;
    const char *_instance;
    uint16_t _port;
    uint32_t _ip = 0;
    uint32_t _announceAt = 0;
    uint8_t _pairs = 0;
    uint8_t _used = 0;
    const char *_txt[2 * MAX_TXT];
    char _arena[ARENA];
};
