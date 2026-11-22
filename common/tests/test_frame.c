#include <stdio.h>
#include <string.h>

#include "uav/protocol/frame.h"
#include "uav/protocol/messages.h"

static int failures = 0;
#define CHECK(cond)                                                                  \
    do                                                                               \
    {                                                                                \
        if (!(cond))                                                                 \
        {                                                                            \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                              \
        }                                                                            \
    } while (0)

static void test_crc_known_vector(void)
{
    /* CRC-16/CCITT-FALSE of "123456789" is 0x29B1. */
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(uav_crc16(data, sizeof(data)) == 0x29B1u);
    const uint8_t empty[1] = {0};
    CHECK(uav_crc16(empty, 0) == 0xFFFFu);
}

static void test_roundtrip(void)
{
    uav_msg_heartbeat_t hb = {UAV_NODE_FLIGHT_CONTROLLER, UAV_PROTOCOL_VERSION, 0x0003u, 123456u};
    uint8_t wire[UAV_FRAME_MAX_SIZE];
    size_t n = uav_frame_encode(UAV_MSG_HEARTBEAT, 7, &hb, sizeof(hb), wire, sizeof(wire));
    CHECK(n == UAV_FRAME_OVERHEAD + sizeof(hb));

    uav_frame_parser_t parser;
    uav_frame_parser_init(&parser);
    uav_frame_t frame;
    int got = 0;
    /* Leading garbage must be skipped. */
    const uint8_t noise[] = {0x00, 0xA5, 0x11, 0xFF};
    for (size_t i = 0; i < sizeof(noise); ++i)
    {
        got += uav_frame_parser_feed(&parser, noise[i], &frame);
    }
    for (size_t i = 0; i < n; ++i)
    {
        got += uav_frame_parser_feed(&parser, wire[i], &frame);
    }
    CHECK(got == 1);
    CHECK(frame.msg_id == UAV_MSG_HEARTBEAT);
    CHECK(frame.seq == 7);
    CHECK(frame.len == sizeof(hb));
    CHECK(memcmp(frame.payload, &hb, sizeof(hb)) == 0);
}

static void test_crc_error_rejected(void)
{
    uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t wire[UAV_FRAME_MAX_SIZE];
    size_t n = uav_frame_encode(UAV_MSG_IMU, 0, payload, sizeof(payload), wire, sizeof(wire));
    wire[UAV_FRAME_HEADER_SIZE] ^= 0xFFu; /* corrupt payload */

    uav_frame_parser_t parser;
    uav_frame_parser_init(&parser);
    uav_frame_t frame;
    int got = 0;
    for (size_t i = 0; i < n; ++i)
    {
        got += uav_frame_parser_feed(&parser, wire[i], &frame);
    }
    CHECK(got == 0);
    CHECK(parser.crc_errors == 1);
}

static void test_oversize_payload_rejected(void)
{
    uint8_t big[UAV_FRAME_MAX_PAYLOAD + 1] = {0};
    uint8_t wire[UAV_FRAME_MAX_SIZE + 1];
    CHECK(uav_frame_encode(UAV_MSG_IMU, 0, big, sizeof(big), wire, sizeof(wire)) == 0);
}

int main(void)
{
    test_crc_known_vector();
    test_roundtrip();
    test_crc_error_rejected();
    test_oversize_payload_rejected();
    if (failures)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all frame tests passed\n");
    return 0;
}
