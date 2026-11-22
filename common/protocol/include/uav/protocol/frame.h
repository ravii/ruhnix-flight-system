/*
 * Binary framing used on serial links (FC <-> companion computer).
 *
 *   +------+------+--------+-----+---------+-----------+-----------+
 *   | 0xA5 | 0x5A | msg_id | seq | len(LE) | payload   | crc16(LE) |
 *   |  1B  |  1B  |   1B   | 1B  |   2B    | len bytes |    2B     |
 *   +------+------+--------+-----+---------+-----------+-----------+
 *
 * The CRC (CRC-16/CCITT-FALSE) covers msg_id .. end of payload.
 * No dynamic allocation; safe to use from RTOS tasks.
 */
#ifndef UAV_PROTOCOL_FRAME_H
#define UAV_PROTOCOL_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UAV_FRAME_SYNC0 0xA5u
#define UAV_FRAME_SYNC1 0x5Au
#define UAV_FRAME_HEADER_SIZE 6u
#define UAV_FRAME_CRC_SIZE 2u
#define UAV_FRAME_MAX_PAYLOAD 128u
#define UAV_FRAME_OVERHEAD (UAV_FRAME_HEADER_SIZE + UAV_FRAME_CRC_SIZE)
#define UAV_FRAME_MAX_SIZE (UAV_FRAME_OVERHEAD + UAV_FRAME_MAX_PAYLOAD)

uint16_t uav_crc16(const uint8_t* data, size_t len);

/* Encodes one frame into `out`. Returns the number of bytes written,
 * or 0 if the payload is too large or `out_size` is insufficient. */
size_t uav_frame_encode(uint8_t msg_id, uint8_t seq, const void* payload, uint16_t payload_len,
    uint8_t* out, size_t out_size);

typedef struct
{
    uint8_t msg_id;
    uint8_t seq;
    uint16_t len;
    uint8_t payload[UAV_FRAME_MAX_PAYLOAD];
} uav_frame_t;

typedef struct
{
    uint8_t buf[UAV_FRAME_MAX_SIZE];
    size_t pos;
    uint32_t crc_errors;
} uav_frame_parser_t;

void uav_frame_parser_init(uav_frame_parser_t* p);

/* Feeds one byte. Returns 1 and fills `out` when a complete, valid frame
 * has been received; returns 0 otherwise. */
int uav_frame_parser_feed(uav_frame_parser_t* p, uint8_t byte, uav_frame_t* out);

#ifdef __cplusplus
}
#endif

#endif /* UAV_PROTOCOL_FRAME_H */
