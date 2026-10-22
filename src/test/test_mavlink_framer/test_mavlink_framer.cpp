#include <cstdint>
#include <vector>
#include <MavlinkFramer.h>
#include <unity.h>

using namespace std;

typedef vector<uint8_t> bytes;

static vector<bytes> sent;

static void send(const uint8_t *data, uint16_t len)
{
    sent.push_back(bytes(data, data + len));
}

// A MAVLink v1 frame with a payload of payloadLen bytes. The CRC is not correct.
static bytes frameV1(uint8_t payloadLen, uint8_t fill)
{
    bytes f = {0xFE, payloadLen, 0, 1, 1, 0};
    f.insert(f.end(), payloadLen + 2, fill);
    return f;
}

// A MAVLink v2 frame with a payload of payloadLen bytes. The CRC is not correct.
static bytes frameV2(uint8_t payloadLen, uint8_t fill, bool signedFrame = false)
{
    bytes f = {0xFD, payloadLen, (uint8_t)(signedFrame ? 1 : 0), 0, 0, 1, 1, 0, 0, 0};
    f.insert(f.end(), payloadLen + 2 + (signedFrame ? 13 : 0), fill);
    return f;
}

template <uint16_t N>
static void pushAll(MavlinkFramer<N> &framer, const bytes &data)
{
    for (uint8_t c : data)
    {
        framer.push(c, send);
    }
}

static bytes join(const vector<bytes> &parts)
{
    bytes out;
    for (const bytes &p : parts)
    {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

void test_frames_of_all_types_pass(void)
{
    MavlinkFramer<512> framer;
    const vector<bytes> frames = {frameV1(9, 0x11), frameV2(9, 0x22), frameV2(0, 0x33), frameV2(20, 0x44, true), frameV1(0, 0x55)};
    pushAll(framer, join(frames));
    TEST_ASSERT_EQUAL(0, sent.size());

    framer.flush(send);
    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == join(frames));
}

void test_noise_between_frames_is_discarded(void)
{
    MavlinkFramer<512> framer;
    const bytes a = frameV2(5, 0xAA);
    const bytes b = frameV1(5, 0xBB);
    pushAll(framer, {0x00, 0x12, 0x34});
    pushAll(framer, a);
    pushAll(framer, {0x55, 0x66});
    pushAll(framer, b);
    framer.flush(send);

    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == join({a, b}));
}

void test_flush_keeps_partial_frame(void)
{
    MavlinkFramer<512> framer;
    const bytes a = frameV2(10, 0xAA);
    const bytes b = frameV2(10, 0xBB);
    pushAll(framer, a);
    pushAll(framer, bytes(b.begin(), b.begin() + 7));

    framer.flush(send);
    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == a);

    // Nothing complete to send
    framer.flush(send);
    TEST_ASSERT_EQUAL(1, sent.size());

    pushAll(framer, bytes(b.begin() + 7, b.end()));
    framer.flush(send);
    TEST_ASSERT_EQUAL(2, sent.size());
    TEST_ASSERT_TRUE(sent[1] == b);
}

void test_partial_header_is_kept(void)
{
    MavlinkFramer<512> framer;
    const bytes a = frameV2(3, 0xAA, true);
    for (size_t i = 0; i < a.size(); ++i)
    {
        framer.push(a[i], send);
        framer.flush(send);
    }
    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == a);
}

void test_full_buffer_sends_before_overflow(void)
{
    // Only one frame of the maximum length fits
    MavlinkFramer<300> framer;
    const bytes a = frameV2(255, 0xAA, true);
    const bytes b = frameV2(255, 0xBB, true);
    const bytes c = frameV1(1, 0xCC);
    TEST_ASSERT_EQUAL(280, a.size());

    pushAll(framer, a);
    TEST_ASSERT_EQUAL(0, sent.size());
    pushAll(framer, b);
    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == a);
    // The framer keeps space for a frame of the maximum length, thus c also sends b
    pushAll(framer, c);
    TEST_ASSERT_EQUAL(2, sent.size());
    TEST_ASSERT_TRUE(sent[1] == b);

    framer.flush(send);
    TEST_ASSERT_EQUAL(3, sent.size());
    TEST_ASSERT_TRUE(sent[2] == c);
}

void test_stx_byte_inside_frame(void)
{
    MavlinkFramer<512> framer;
    // Payloads full of STX bytes must not start new frames
    const bytes a = frameV2(30, 0xFD);
    const bytes b = frameV1(30, 0xFE);
    pushAll(framer, join({a, b}));
    framer.flush(send);
    TEST_ASSERT_EQUAL(1, sent.size());
    TEST_ASSERT_TRUE(sent[0] == join({a, b}));
}

// Unity setup/teardown
void setUp()
{
    sent.clear();
}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_frames_of_all_types_pass);
    RUN_TEST(test_noise_between_frames_is_discarded);
    RUN_TEST(test_flush_keeps_partial_frame);
    RUN_TEST(test_partial_header_is_kept);
    RUN_TEST(test_full_buffer_sends_before_overflow);
    RUN_TEST(test_stx_byte_inside_frame);
    UNITY_END();

    return 0;
}
