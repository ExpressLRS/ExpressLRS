#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unity.h>

#include "MiniMDNS.h"

void setUp() {}
void tearDown() {}

enum : uint16_t { T_A = 1, T_PTR = 12, T_TXT = 16, T_SRV = 33, T_ANY = 255 };

static uint8_t pkt[512];
static uint8_t out[1024];

static MiniMDNS *mdns;

static void makeResponder()
{
    delete mdns;
    mdns = new MiniMDNS("elrs_rx", 80);
    mdns->addTxt("vendor", "elrs");
    mdns->addTxt("type", "rx");
    mdns->addTxt("options", mdns->store("-DAUTO_WIFI_ON_INTERVAL=60 -DRCVR_UART_BAUD=420000"));
}

/*
 * Packet helpers
 */

static size_t putName(uint8_t *p, const char *name)
{
    size_t o = 0;
    while (*name)
    {
        const char *dot = strchr(name, '.');
        size_t l = dot ? (size_t)(dot - name) : strlen(name);
        p[o++] = l;
        memcpy(p + o, name, l);
        o += l;
        name += l;
        if (*name == '.')
        {
            name++;
        }
    }
    p[o++] = 0;
    return o;
}

static size_t header(uint8_t *p, uint16_t questions, bool response = false)
{
    memset(p, 0, 12);
    p[2] = response ? 0x80 : 0;
    p[5] = questions;
    return 12;
}

static size_t question(uint8_t *p, size_t o, const char *name, uint16_t type)
{
    o += putName(p + o, name);
    p[o++] = type >> 8;
    p[o++] = type;
    p[o++] = 0;
    p[o++] = 1;
    return o;
}

static size_t ask(const char *name, uint16_t type)
{
    size_t len = question(pkt, header(pkt, 1), name, type);
    return mdns->testHandle(pkt, len, out, sizeof(out));
}

struct RR
{
    std::string name;
    uint16_t type;
    uint16_t cls;
    uint32_t ttl;
    std::vector<uint8_t> rdata;
};

struct Reply
{
    uint16_t flags;
    std::vector<RR> answers;
    std::vector<RR> additionals;
};

static std::string readName(const uint8_t *p, size_t &pos)
{
    std::string s;
    size_t jump = 0;
    bool jumped = false;
    while (true)
    {
        uint8_t l = p[pos];
        if ((l & 0xC0) == 0xC0)
        {
            size_t ptr = ((l & 0x3F) << 8) | p[pos + 1];
            if (!jumped)
            {
                jump = pos + 2;
            }
            jumped = true;
            pos = ptr;
            continue;
        }
        pos++;
        if (l == 0)
        {
            break;
        }
        if (!s.empty())
        {
            s += '.';
        }
        s.append((const char *)p + pos, l);
        pos += l;
    }
    if (jumped)
    {
        pos = jump;
    }
    return s;
}

static uint16_t rd16(const uint8_t *p, size_t pos)
{
    return (p[pos] << 8) | p[pos + 1];
}

static RR readRR(const uint8_t *p, size_t &pos)
{
    RR rr;
    rr.name = readName(p, pos);
    rr.type = rd16(p, pos);
    rr.cls = rd16(p, pos + 2);
    rr.ttl = ((uint32_t)rd16(p, pos + 4) << 16) | rd16(p, pos + 6);
    uint16_t rdlen = rd16(p, pos + 8);
    pos += 10;
    rr.rdata.assign(p + pos, p + pos + rdlen);
    pos += rdlen;
    return rr;
}

static Reply parse(size_t len)
{
    Reply r;
    TEST_ASSERT_GREATER_OR_EQUAL(12, len);
    r.flags = rd16(out, 2);
    TEST_ASSERT_EQUAL(0, rd16(out, 4)); // no questions echoed
    TEST_ASSERT_EQUAL(0, rd16(out, 8)); // no authority records
    uint16_t an = rd16(out, 6);
    uint16_t ar = rd16(out, 10);
    size_t pos = 12;
    for (uint16_t i = 0; i < an; i++)
    {
        r.answers.push_back(readRR(out, pos));
    }
    for (uint16_t i = 0; i < ar; i++)
    {
        r.additionals.push_back(readRR(out, pos));
    }
    TEST_ASSERT_EQUAL(len, pos); // nothing left over
    return r;
}

static const RR *find(const std::vector<RR> &rrs, uint16_t type)
{
    for (const RR &rr : rrs)
    {
        if (rr.type == type)
        {
            return &rr;
        }
    }
    return nullptr;
}

static std::string rdataName(const RR &rr, size_t offset = 0)
{
    size_t pos = offset;
    return readName(rr.rdata.data(), pos);
}

static std::vector<std::string> txtItems(const RR &rr)
{
    std::vector<std::string> items;
    size_t pos = 0;
    while (pos < rr.rdata.size())
    {
        uint8_t l = rr.rdata[pos++];
        items.push_back(std::string((const char *)rr.rdata.data() + pos, l));
        pos += l;
    }
    return items;
}

static const char *INSTANCE = "elrs_rx_AABBCC010203._http._tcp.local";

/*
 * Tests
 */

void test_instance_name()
{
    TEST_ASSERT_EQUAL_STRING("elrs_rx_AABBCC010203", mdns->instance());
}

void test_announce_has_everything()
{
    Reply r = parse(mdns->testAnnounce(out, sizeof(out), false));
    TEST_ASSERT_EQUAL_HEX16(0x8400, r.flags);
    TEST_ASSERT_EQUAL(5, r.answers.size());
    TEST_ASSERT_EQUAL(0, r.additionals.size());

    const RR *a = find(r.answers, T_A);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_STRING("elrs_rx.local", a->name.c_str());
    TEST_ASSERT_EQUAL_HEX16(0x8001, a->cls);
    TEST_ASSERT_EQUAL(120, a->ttl);
    const uint8_t ip[4] = {10, 0, 0, 1};
    TEST_ASSERT_EQUAL(4, a->rdata.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ip, a->rdata.data(), 4);

    const RR *srv = find(r.answers, T_SRV);
    TEST_ASSERT_NOT_NULL(srv);
    TEST_ASSERT_EQUAL_STRING(INSTANCE, srv->name.c_str());
    TEST_ASSERT_EQUAL_HEX16(0x8001, srv->cls);
    TEST_ASSERT_EQUAL(80, rd16(srv->rdata.data(), 4));
    TEST_ASSERT_EQUAL_STRING("elrs_rx.local", rdataName(*srv, 6).c_str());

    const RR *txt = find(r.answers, T_TXT);
    TEST_ASSERT_NOT_NULL(txt);
    TEST_ASSERT_EQUAL_STRING(INSTANCE, txt->name.c_str());
    TEST_ASSERT_EQUAL_HEX16(0x8001, txt->cls);
    TEST_ASSERT_EQUAL(4500, txt->ttl);
    std::vector<std::string> items = txtItems(*txt);
    TEST_ASSERT_EQUAL(3, items.size());
    TEST_ASSERT_EQUAL_STRING("vendor=elrs", items[0].c_str());
    TEST_ASSERT_EQUAL_STRING("type=rx", items[1].c_str());
    TEST_ASSERT_EQUAL_STRING("options=-DAUTO_WIFI_ON_INTERVAL=60 -DRCVR_UART_BAUD=420000", items[2].c_str());

    // two PTRs: service enumeration and the service itself, both shared records
    int ptrs = 0;
    for (const RR &rr : r.answers)
    {
        if (rr.type != T_PTR)
        {
            continue;
        }
        ptrs++;
        TEST_ASSERT_EQUAL_HEX16(0x0001, rr.cls);
        if (rr.name == "_services._dns-sd._udp.local")
        {
            TEST_ASSERT_EQUAL_STRING("_http._tcp.local", rdataName(rr).c_str());
        }
        else
        {
            TEST_ASSERT_EQUAL_STRING("_http._tcp.local", rr.name.c_str());
            TEST_ASSERT_EQUAL_STRING(INSTANCE, rdataName(rr).c_str());
        }
    }
    TEST_ASSERT_EQUAL(2, ptrs);
}

void test_goodbye_zero_ttl()
{
    Reply r = parse(mdns->testAnnounce(out, sizeof(out), true));
    TEST_ASSERT_EQUAL(5, r.answers.size());
    for (const RR &rr : r.answers)
    {
        TEST_ASSERT_EQUAL(0, rr.ttl);
    }
}

void test_service_browse()
{
    Reply r = parse(ask("_http._tcp.local", T_PTR));
    TEST_ASSERT_EQUAL(1, r.answers.size());
    TEST_ASSERT_EQUAL(T_PTR, r.answers[0].type);
    TEST_ASSERT_EQUAL_STRING(INSTANCE, rdataName(r.answers[0]).c_str());
    TEST_ASSERT_EQUAL(3, r.additionals.size());
    TEST_ASSERT_NOT_NULL(find(r.additionals, T_SRV));
    TEST_ASSERT_NOT_NULL(find(r.additionals, T_TXT));
    TEST_ASSERT_NOT_NULL(find(r.additionals, T_A));
}

void test_host_lookup_case_insensitive()
{
    Reply r = parse(ask("ELRS_RX.Local", T_A));
    TEST_ASSERT_EQUAL(1, r.answers.size());
    TEST_ASSERT_EQUAL(T_A, r.answers[0].type);
    TEST_ASSERT_EQUAL(0, r.additionals.size());
}

void test_srv_brings_address()
{
    Reply r = parse(ask(INSTANCE, T_SRV));
    TEST_ASSERT_EQUAL(1, r.answers.size());
    TEST_ASSERT_EQUAL(T_SRV, r.answers[0].type);
    TEST_ASSERT_EQUAL(1, r.additionals.size());
    TEST_ASSERT_EQUAL(T_A, r.additionals[0].type);
}

void test_txt_only()
{
    Reply r = parse(ask(INSTANCE, T_TXT));
    TEST_ASSERT_EQUAL(1, r.answers.size());
    TEST_ASSERT_EQUAL(T_TXT, r.answers[0].type);
    TEST_ASSERT_EQUAL(0, r.additionals.size());
}

void test_any_on_instance()
{
    Reply r = parse(ask(INSTANCE, T_ANY));
    TEST_ASSERT_EQUAL(2, r.answers.size());
    TEST_ASSERT_NOT_NULL(find(r.answers, T_SRV));
    TEST_ASSERT_NOT_NULL(find(r.answers, T_TXT));
    TEST_ASSERT_EQUAL(1, r.additionals.size());
    TEST_ASSERT_EQUAL(T_A, r.additionals[0].type);
}

void test_service_enumeration()
{
    Reply r = parse(ask("_services._dns-sd._udp.local", T_PTR));
    TEST_ASSERT_EQUAL(1, r.answers.size());
    TEST_ASSERT_EQUAL_STRING("_http._tcp.local", rdataName(r.answers[0]).c_str());
    TEST_ASSERT_EQUAL(0, r.additionals.size());
}

void test_compressed_second_question()
{
    // q1 is the full service name, q2 points back at its "local" label
    size_t o = header(pkt, 2);
    size_t localAt = o + 6 + 5; // past "_http" and "_tcp"
    o = question(pkt, o, "_http._tcp.local", T_PTR);
    o += putName(pkt + o, "_services._dns-sd._udp") - 1; // drop the root label
    pkt[o++] = 0xC0 | (localAt >> 8);
    pkt[o++] = localAt;
    pkt[o++] = 0;
    pkt[o++] = T_PTR;
    pkt[o++] = 0;
    pkt[o++] = 1;
    Reply r = parse(mdns->testHandle(pkt, o, out, sizeof(out)));
    TEST_ASSERT_EQUAL(2, r.answers.size());
    TEST_ASSERT_EQUAL(3, r.additionals.size());
}

void test_wrong_type_ignored()
{
    TEST_ASSERT_EQUAL(0, ask("elrs_rx.local", T_SRV));
    TEST_ASSERT_EQUAL(0, ask("_http._tcp.local", T_A));
}

void test_other_names_ignored()
{
    TEST_ASSERT_EQUAL(0, ask("elrs_tx.local", T_A));
    TEST_ASSERT_EQUAL(0, ask("_ssh._tcp.local", T_PTR));
    TEST_ASSERT_EQUAL(0, ask("elrs_rx.local.extra", T_A));
}

void test_responses_ignored()
{
    size_t len = question(pkt, header(pkt, 1, true), "elrs_rx.local", T_A);
    TEST_ASSERT_EQUAL(0, mdns->testHandle(pkt, len, out, sizeof(out)));
}

void test_truncated_packets()
{
    size_t len = question(pkt, header(pkt, 1), "elrs_rx.local", T_A);
    for (size_t cut = 0; cut < len; cut++)
    {
        TEST_ASSERT_EQUAL(0, mdns->testHandle(pkt, cut, out, sizeof(out)));
    }
}

void test_pointer_loop()
{
    size_t o = header(pkt, 1);
    pkt[o++] = 0xC0; // points at itself
    pkt[o++] = 12;
    pkt[o++] = 0;
    pkt[o++] = T_A;
    pkt[o++] = 0;
    pkt[o++] = 1;
    TEST_ASSERT_EQUAL(0, mdns->testHandle(pkt, o, out, sizeof(out)));
}

void test_long_txt_capped()
{
    delete mdns;
    mdns = new MiniMDNS("elrs_rx", 80);
    static char big[300];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    mdns->addTxt("k", big);
    Reply r = parse(ask(INSTANCE, T_TXT));
    std::vector<std::string> items = txtItems(r.answers[0]);
    TEST_ASSERT_EQUAL(1, items.size());
    TEST_ASSERT_EQUAL(255, items[0].size());
    TEST_ASSERT_EQUAL_STRING_LEN("k=xxx", items[0].c_str(), 5);
    makeResponder();
}

void test_arena_full()
{
    delete mdns;
    mdns = new MiniMDNS("elrs_rx", 80);
    static char big[MiniMDNS::ARENA];
    memset(big, 'y', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    TEST_ASSERT_NULL(mdns->store(big)); // the instance name is already in there
    TEST_ASSERT_NOT_NULL(mdns->store("fits"));
    makeResponder();
}

int main(int argc, char **argv)
{
    makeResponder();
    UNITY_BEGIN();
    RUN_TEST(test_instance_name);
    RUN_TEST(test_announce_has_everything);
    RUN_TEST(test_goodbye_zero_ttl);
    RUN_TEST(test_service_browse);
    RUN_TEST(test_host_lookup_case_insensitive);
    RUN_TEST(test_srv_brings_address);
    RUN_TEST(test_txt_only);
    RUN_TEST(test_any_on_instance);
    RUN_TEST(test_service_enumeration);
    RUN_TEST(test_compressed_second_question);
    RUN_TEST(test_wrong_type_ignored);
    RUN_TEST(test_other_names_ignored);
    RUN_TEST(test_responses_ignored);
    RUN_TEST(test_truncated_packets);
    RUN_TEST(test_pointer_loop);
    RUN_TEST(test_long_txt_capped);
    RUN_TEST(test_arena_full);
    UNITY_END();
    return 0;
}
