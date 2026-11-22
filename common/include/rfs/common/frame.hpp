/**
 * Binary framing used on serial links (FC <-> companion computer, FC <-> simulator).
 *
 *   +------+------+--------+-----+---------+-----------+-----------+
 *   | 0xA5 | 0x5A | msg_id | seq | len(LE) | payload   | crc16(LE) |
 *   |  1B  |  1B  |   1B   | 1B  |   2B    | len bytes |    2B     |
 *   +------+------+--------+-----+---------+-----------+-----------+
 *
 * The CRC (CRC-16/CCITT-FALSE) covers msg_id .. end of payload.
 * No dynamic allocation, no exceptions; safe to use from RTOS tasks.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "rfs/common/messages.hpp"

namespace rfs::common
{

inline constexpr uint8_t kFrameSync0 = 0xA5;
inline constexpr uint8_t kFrameSync1 = 0x5A;
inline constexpr std::size_t kFrameHeaderSize = 6;
inline constexpr std::size_t kFrameCrcSize = 2;
inline constexpr std::size_t kFrameMaxPayload = 128;
inline constexpr std::size_t kFrameOverhead = kFrameHeaderSize + kFrameCrcSize;
inline constexpr std::size_t kFrameMaxSize = kFrameOverhead + kFrameMaxPayload;

uint16_t crc16(const uint8_t* data, std::size_t len);

/**
 * Encodes one frame into `out`.
 * @return the number of bytes written, or 0 if the payload is larger than
 *         kFrameMaxPayload or `out_size` is too small.
 */
std::size_t frame_encode(MsgId msg_id, uint8_t seq, const void* payload, std::size_t payload_len,
    uint8_t* out, std::size_t out_size);

struct Frame
{
    MsgId msg_id{};
    uint8_t seq = 0;
    uint16_t len = 0;
    std::array<uint8_t, kFrameMaxPayload> payload{};
};

/** Byte-wise frame parser: resynchronizes on garbage and drops frames with a bad CRC. */
class FrameParser
{
public:
    /** Feeds one byte. @return true when `out` holds a complete, valid frame. */
    bool feed(uint8_t byte, Frame& out);

    void reset() { pos_ = 0; }

    uint32_t crc_errors() const { return crc_errors_; }

private:
    std::array<uint8_t, kFrameMaxSize> buf_{};
    std::size_t pos_ = 0;
    uint32_t crc_errors_ = 0;
};

}  // namespace rfs::common
