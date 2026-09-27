#include <MavLuaBridge.h>
#include <unity.h>
#include <vector>
#include <stdio.h>
#include <cstring>

static std::vector<uint8_t> message(uint8_t id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> p = {0, uint8_t(payload.size()+8), 254, uint8_t(payload.size()), 0, 254, 190, id};
    p.insert(p.end(),payload.begin(),payload.end());
    uint16_t crc = 65535;
    for (size_t i=3; i<=p.size(); ++i) {
        crc ^= i == p.size() ? (id == 4 ? 237 : id == 20 ? 214 : id == 23 ? 168 : 152) : p[i];
        for (unsigned j=0;j<8;++j) crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);
    }
    p.push_back(crc & 255); p.push_back(crc >> 8);
    return p;
}
static void test_mavlua_protocol() {
    MavLuaBridge b;
    uint8_t hb[9] = {0,0,0,0,1,3,1,3,3};
    uint8_t param[25] = {}; param[6]=44; param[7]=1; // index 300
    auto ping=message(4,std::vector<uint8_t>(14,0));
    std::vector<uint8_t> readPayload(20,0); readPayload[0]=44; readPayload[1]=1; readPayload[2]=17; readPayload[3]=1;
    auto read=message(20,readPayload);
    std::vector<uint8_t> setPayload(23,0); setPayload[4]=17; setPayload[5]=1; setPayload[22]=9;
    auto write=message(23,setPayload);
    TEST_ASSERT_TRUE(!b.subscribed(0));
    TEST_ASSERT_TRUE(!b.downlink(0,17,1,hb,0));
    TEST_ASSERT_TRUE(!b.input(read.data(),read.size(),0));
    TEST_ASSERT_TRUE(!b.input(ping.data(),ping.size(),100));
    TEST_ASSERT_TRUE(b.subscribed(100));
    TEST_ASSERT_TRUE(!b.input(write.data(),write.size(),100)); // no discovered heartbeat
    TEST_ASSERT_TRUE(!b.downlink(0,17,190,hb,200));
    hb[5]=12; TEST_ASSERT_TRUE(!b.downlink(0,17,1,hb,200)); hb[5]=3; // PX4 is not C-cast
    TEST_ASSERT_TRUE(b.downlink(0,17,1,hb,200));
    TEST_ASSERT_TRUE(!b.downlink(0,18,1,hb,200));
    TEST_ASSERT_TRUE(b.input(read.data(),read.size(),300)==28);
    TEST_ASSERT_TRUE(!b.input(read.data(),read.size(),301)); // rate limit
    TEST_ASSERT_TRUE(!b.downlink(22,18,1,param,350));
    param[6]=43; TEST_ASSERT_TRUE(!b.downlink(22,17,1,param,350)); param[6]=44;
    TEST_ASSERT_TRUE(b.downlink(22,17,1,param,350));
    TEST_ASSERT_TRUE(!b.downlink(22,17,1,param,350)); // one response only, no flood
    TEST_ASSERT_TRUE(b.input(write.data(),write.size(),400)==31);
    hb[6]=129; TEST_ASSERT_TRUE(b.downlink(0,17,1,hb,500));
    TEST_ASSERT_TRUE(b.input(write.data(),write.size(),600)==31);
    hb[6]=1; TEST_ASSERT_TRUE(b.downlink(0,17,1,hb,600));
    TEST_ASSERT_TRUE(!b.input(write.data(),write.size(),4000)); // stale heartbeat
    TEST_ASSERT_TRUE(b.input(read.data(),read.size(),4000)==28); // read still allowed
    auto bad=read; bad.back()^=1; TEST_ASSERT_TRUE(!b.input(bad.data(),bad.size(),5000));
    bad=read; bad[0]=1; TEST_ASSERT_TRUE(!b.input(bad.data(),bad.size(),5000));
    for (size_t i=0;i<read.size();++i) TEST_ASSERT_TRUE(!b.input(read.data(),i,5000));
    readPayload[2]=18; bad=message(20,readPayload); TEST_ASSERT_TRUE(!b.input(bad.data(),bad.size(),5000));
    TEST_ASSERT_TRUE(!b.subscribed(14000));
    TEST_ASSERT_TRUE(!b.downlink(22,17,1,param,14000));
    TEST_ASSERT_TRUE(!b.input(write.data(),write.size(),14000));
    b.reset();
    TEST_ASSERT_TRUE(!b.input(ping.data(),ping.size(),0xFFFFFF00));
    TEST_ASSERT_TRUE(b.subscribed(0x50)); // millis wrap
    TEST_ASSERT_TRUE(b.downlink(0,17,1,hb,0x50));
    TEST_ASSERT_TRUE(b.input(write.data(),write.size(),0x100)==31);
    static_assert(sizeof(MavLuaBridge) <= 80, "bridge retained state must remain bounded");
    printf("PASS: bridge framing, CRC, target selection, rate limit, subscription expiry, write staleness and clock wrap\n");
}

static std::vector<uint8_t> readIndex(uint16_t index) {
    std::vector<uint8_t> p(20, 0);
    p[0] = index & 255; p[1] = index >> 8; p[2] = 17; p[3] = 1;
    return message(20, p);
}

static void test_named_read_and_version() {
    MavLuaBridge b;
    auto ping = message(4, std::vector<uint8_t>(14, 0));
    uint8_t hb[9] = {0,0,0,0,2,3,1,3,3}, p[78] = {};
    b.input(ping.data(), ping.size(), 100);
    TEST_ASSERT_TRUE(b.downlink(0, 17, 1, hb, 110));

    std::vector<uint8_t> namedPayload(20, 0);
    namedPayload[0] = namedPayload[1] = 255;
    namedPayload[2] = 17; namedPayload[3] = 1;
    memcpy(namedPayload.data() + 4, "ATC_ANG_RLL_P", 13);
    auto named = message(20, namedPayload);
    TEST_ASSERT_EQUAL(28, b.input(named.data(), named.size(), 200));
    memcpy(p + 8, "ATC_ANG_RLL_P", 13);
    p[6] = p[7] = 255; // ArduPilot echoes named reads with param_index=-1.
    TEST_ASSERT_TRUE(b.downlink(22, 17, 1, p, 220));
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 220));
    namedPayload[4] = '-';
    auto invalid = message(20, namedPayload);
    TEST_ASSERT_EQUAL(0, b.input(invalid.data(), invalid.size(), 300));

    std::vector<uint8_t> versionPayload(33, 0);
    versionPayload[2] = 0x14; versionPayload[3] = 0x43; // float32(148)
    versionPayload[29] = 2; // MAV_CMD_REQUEST_MESSAGE = 512
    versionPayload[30] = 17; versionPayload[31] = 1;
    auto version = message(76, versionPayload);
    TEST_ASSERT_EQUAL(41, b.input(version.data(), version.size(), 400));
    TEST_ASSERT_FALSE(b.downlink(148, 17, 2, p, 440));
    TEST_ASSERT_TRUE(b.downlink(148, 17, 1, p, 450));
    TEST_ASSERT_FALSE(b.downlink(148, 17, 1, p, 450));
    versionPayload[4] = 1;
    auto ambiguous = message(76, versionPayload);
    TEST_ASSERT_EQUAL(0, b.input(ambiguous.data(), ambiguous.size(), 600));
}
static void test_pipeline_window() {
    MavLuaBridge b;
    auto ping = message(4, std::vector<uint8_t>(14, 0));
    uint8_t hb[9] = {0,0,0,0,1,3,1,3,3}, p[25] = {};
    b.input(ping.data(), ping.size(), 100);
    TEST_ASSERT_TRUE(b.downlink(0, 17, 1, hb, 110));
    for (unsigned i = 0; i < 4; ++i) {
        auto r = readIndex(i);
        TEST_ASSERT_EQUAL(28, b.input(r.data(), r.size(), 200 + i * 40));
    }
    auto fifth = readIndex(4), retry = readIndex(1);
    TEST_ASSERT_EQUAL(0, b.input(fifth.data(), fifth.size(), 360)); // bounded window
    TEST_ASSERT_EQUAL(28, b.input(retry.data(), retry.size(), 360)); // no extra slot
    p[6] = 2;
    TEST_ASSERT_FALSE(b.downlink(22, 18, 1, p, 380));
    TEST_ASSERT_TRUE(b.downlink(22, 17, 1, p, 380)); // out of order
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 380)); // duplicate
    TEST_ASSERT_EQUAL(28, b.input(fifth.data(), fifth.size(), 400));
    for (unsigned i : {4u, 0u, 3u, 1u}) {
        p[6] = i;
        TEST_ASSERT_TRUE(b.downlink(22, 17, 1, p, 450));
    }
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 450));
    auto r = readIndex(0);
    TEST_ASSERT_EQUAL(0, b.input(r.data(), r.size(), 500, 27)); // full uplink
    p[6] = 0;
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 500)); // not falsely reserved
    TEST_ASSERT_EQUAL(28, b.input(r.data(), r.size(), 500, 28));
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 5500)); // stale read
    for (unsigned i = 0; i < 4; ++i) {
        auto q = readIndex(i);
        TEST_ASSERT_EQUAL(28, b.input(q.data(), q.size(), 5600 + i * 40));
    }
    b.input(ping.data(), ping.size(), 10000); // keep lease while abandoned reads expire
    TEST_ASSERT_EQUAL(28, b.input(fifth.data(), fifth.size(), 11000));
    b.reset();
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 11010));
}
static void test_pipeline_write_gate() {
    MavLuaBridge b;
    auto ping = message(4, std::vector<uint8_t>(14, 0)), r = readIndex(0);
    uint8_t hb[9] = {0,0,0,0,1,3,1,3,3}, p[25] = {};
    b.input(ping.data(), ping.size(), 100);
    b.downlink(0, 17, 1, hb, 110);
    TEST_ASSERT_EQUAL(28, b.input(r.data(), r.size(), 200));
    std::vector<uint8_t> payload(23, 0); payload[4] = 17; payload[5] = 1; payload[22] = 9;
    auto w = message(23, payload);
    TEST_ASSERT_EQUAL(0, b.input(w.data(), w.size(), 250));
    TEST_ASSERT_EQUAL(31, b.input(w.data(), w.size(), 300));
    TEST_ASSERT_FALSE(b.downlink(22, 17, 1, p, 310)); // write clears old subscriptions
    TEST_ASSERT_EQUAL(0, b.input(r.data(), r.size(), 350));
    TEST_ASSERT_EQUAL(28, b.input(r.data(), r.size(), 400));
    TEST_ASSERT_TRUE(b.downlink(22, 17, 1, p, 410)); // explicit verification
    b.reset();
    b.input(ping.data(), ping.size(), 0xFFFFFF00u);
    b.downlink(0, 17, 1, hb, 0xFFFFFF10u);
    TEST_ASSERT_EQUAL(28, b.input(r.data(), r.size(), 0xFFFFFF40u));
    TEST_ASSERT_TRUE(b.downlink(22, 17, 1, p, 0x20u));
}

static void test_readiness_request_pacing() {
    MavLuaReadiness r;
    uint8_t hb[9] = {0,0,0,0,1,3,1,3,3}, status[31] = {};
    TEST_ASSERT_FALSE(r.due(0));
    r.observe(0, 17, 190, hb, 10);
    TEST_ASSERT_FALSE(r.due(10));
    hb[5] = 12; r.observe(0, 17, 1, hb, 10); hb[5] = 3;
    TEST_ASSERT_FALSE(r.due(10));
    r.observe(0, 17, 1, hb, 20);
    TEST_ASSERT_TRUE(r.due(20));
    TEST_ASSERT_EQUAL(17, r.target());
    r.requested(20);
    TEST_ASSERT_FALSE(r.due(1019));
    TEST_ASSERT_TRUE(r.due(1020));
    r.observe(1, 18, 1, status, 1030);
    TEST_ASSERT_TRUE(r.due(1030)); // wrong vehicle cannot suppress requests
    r.requested(1030);
    r.observe(1, 17, 1, status, 1040);
    TEST_ASSERT_FALSE(r.due(3039));
    r.observe(0, 17, 1, hb, 3000);
    TEST_ASSERT_TRUE(r.due(3040));
    r.observe(0, 17, 1, hb, 3050);
    r.requested(3040);
    TEST_ASSERT_FALSE(r.due(6051)); // heartbeat stale
    r.observe(0, 18, 1, hb, 6051); // stale target may be replaced
    TEST_ASSERT_TRUE(r.due(6051));
    TEST_ASSERT_EQUAL(18, r.target());
    r.requested(0xFFFFFF00u);
    r.observe(0, 18, 1, hb, 0xFFFFFF00u);
    TEST_ASSERT_TRUE(r.due(0x00000300u)); // request timer wraps
    r.reset();
    TEST_ASSERT_FALSE(r.due(0x00000300u));
}

void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mavlua_protocol);
    RUN_TEST(test_named_read_and_version);
    RUN_TEST(test_pipeline_window);
    RUN_TEST(test_pipeline_write_gate);
    RUN_TEST(test_readiness_request_pacing);
    return UNITY_END();
}
