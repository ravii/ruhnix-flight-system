#include "uav/protocol/frame.h"

#include <string.h>

uint16_t uav_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (int b = 0; b < 8; ++b) {
            crc = (uint16_t)((crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1));
        }
    }
    return crc;
}

size_t uav_frame_encode(uint8_t msg_id, uint8_t seq, const void *payload, uint16_t payload_len,
                        uint8_t *out, size_t out_size) {
    const size_t total = (size_t)UAV_FRAME_OVERHEAD + payload_len;
    if (payload_len > UAV_FRAME_MAX_PAYLOAD || out_size < total) {
        return 0;
    }
    out[0] = UAV_FRAME_SYNC0;
    out[1] = UAV_FRAME_SYNC1;
    out[2] = msg_id;
    out[3] = seq;
    out[4] = (uint8_t)(payload_len & 0xFFu);
    out[5] = (uint8_t)(payload_len >> 8);
    if (payload_len > 0) {
        memcpy(&out[UAV_FRAME_HEADER_SIZE], payload, payload_len);
    }
    const uint16_t crc = uav_crc16(&out[2], (size_t)4 + payload_len);
    out[UAV_FRAME_HEADER_SIZE + payload_len] = (uint8_t)(crc & 0xFFu);
    out[UAV_FRAME_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc >> 8);
    return total;
}

void uav_frame_parser_init(uav_frame_parser_t *p) {
    memset(p, 0, sizeof(*p));
}

int uav_frame_parser_feed(uav_frame_parser_t *p, uint8_t byte, uav_frame_t *out) {
    /* Hunt for sync bytes. */
    if (p->pos == 0) {
        if (byte == UAV_FRAME_SYNC0) {
            p->buf[p->pos++] = byte;
        }
        return 0;
    }
    if (p->pos == 1) {
        if (byte == UAV_FRAME_SYNC1) {
            p->buf[p->pos++] = byte;
        } else {
            p->pos = (byte == UAV_FRAME_SYNC0) ? 1u : 0u;
        }
        return 0;
    }

    p->buf[p->pos++] = byte;

    if (p->pos < UAV_FRAME_HEADER_SIZE) {
        return 0;
    }
    const uint16_t len = (uint16_t)(p->buf[4] | (p->buf[5] << 8));
    if (len > UAV_FRAME_MAX_PAYLOAD) {
        p->pos = 0; /* Corrupt length; resynchronize. */
        return 0;
    }
    if (p->pos < (size_t)UAV_FRAME_OVERHEAD + len) {
        return 0;
    }

    const uint16_t rx_crc =
        (uint16_t)(p->buf[UAV_FRAME_HEADER_SIZE + len] | (p->buf[UAV_FRAME_HEADER_SIZE + len + 1] << 8));
    const uint16_t calc_crc = uav_crc16(&p->buf[2], (size_t)4 + len);
    p->pos = 0;
    if (rx_crc != calc_crc) {
        p->crc_errors++;
        return 0;
    }

    out->msg_id = p->buf[2];
    out->seq = p->buf[3];
    out->len = len;
    memcpy(out->payload, &p->buf[UAV_FRAME_HEADER_SIZE], len);
    return 1;
}
