#include <cstdio>
#include <cstring>

#include "rfs/common/frame.hpp"
#include "rfs/common/messages.hpp"

namespace
{

int failures = 0;

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

using namespace rfs::common;

void test_crc_known_vector()
{
    // CRC-16/CCITT-FALSE of "123456789" is 0x29B1.
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(crc16(data, sizeof(data)) == 0x29B1u);
    const uint8_t empty[1] = {0};
    CHECK(crc16(empty, 0) == 0xFFFFu);
}

void test_roundtrip()
{
    const HeartbeatMsg hb{NodeId::kFlightController, kProtocolVersion, 0x0003u, 123456u};
    uint8_t wire[kFrameMaxSize];
    const std::size_t n = frame_encode(MsgId::kHeartbeat, 7, &hb, sizeof(hb), wire, sizeof(wire));
    CHECK(n == kFrameOverhead + sizeof(hb));

    FrameParser parser;
    Frame frame;
    int got = 0;
    // Leading garbage must be skipped.
    const uint8_t noise[] = {0x00, 0xA5, 0x11, 0xFF};
    for (uint8_t b : noise)
    {
        got += parser.feed(b, frame) ? 1 : 0;
    }
    for (std::size_t i = 0; i < n; ++i)
    {
        got += parser.feed(wire[i], frame) ? 1 : 0;
    }
    CHECK(got == 1);
    CHECK(frame.msg_id == MsgId::kHeartbeat);
    CHECK(frame.seq == 7);
    CHECK(frame.len == sizeof(hb));
    CHECK(std::memcmp(frame.payload.data(), &hb, sizeof(hb)) == 0);
}

void test_wire_bytes_unchanged()
{
    // Golden frame from the original C implementation: the C++ port must
    // produce identical bytes on the wire.
    const HeartbeatMsg hb{NodeId::kFlightController, kProtocolVersion, 0x0003u, 123456u};
    uint8_t wire[kFrameMaxSize];
    const std::size_t n = frame_encode(MsgId::kHeartbeat, 7, &hb, sizeof(hb), wire, sizeof(wire));
    const uint8_t expected[] = {0xA5, 0x5A, 0x01, 0x07, 0x08, 0x00, 0x01, 0x01, 0x03, 0x00, 0x40,
        0xE2, 0x01, 0x00, 0xD4, 0x46};
    CHECK(n == sizeof(expected));
    CHECK(std::memcmp(wire, expected, sizeof(expected)) == 0);
}

void test_crc_error_rejected()
{
    const uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t wire[kFrameMaxSize];
    const std::size_t n =
        frame_encode(MsgId::kImu, 0, payload, sizeof(payload), wire, sizeof(wire));
    wire[kFrameHeaderSize] ^= 0xFFu;  // corrupt the payload

    FrameParser parser;
    Frame frame;
    int got = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        got += parser.feed(wire[i], frame) ? 1 : 0;
    }
    CHECK(got == 0);
    CHECK(parser.crc_errors() == 1);
}

void test_oversize_payload_rejected()
{
    const uint8_t big[kFrameMaxPayload + 1] = {0};
    uint8_t wire[kFrameMaxSize + 1];
    CHECK(frame_encode(MsgId::kImu, 0, big, sizeof(big), wire, sizeof(wire)) == 0);
}

}  // namespace

int main()
{
    test_crc_known_vector();
    test_roundtrip();
    test_wire_bytes_unchanged();
    test_crc_error_rejected();
    test_oversize_payload_rejected();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all frame tests passed\n");
    return 0;
}
