#include "rfs/common/frame.hpp"

#include <cstring>

namespace rfs::common
{
namespace
{

/** Every message must fit in a single frame. */
template <typename... Msgs>
constexpr bool fits_in_frame()
{
    return ((sizeof(Msgs) <= kFrameMaxPayload) && ...);
}
static_assert(fits_in_frame<HeartbeatMsg, ImuMsg, AttitudeMsg, MotorCmdMsg, PoseEstimateMsg,
                  SimSensorsMsg, SimActuatorsMsg>(),
    "payload too large for one frame");

/** msg_id, seq and the two length bytes are covered by the CRC along with the payload. */
constexpr std::size_t kCrcHeaderBytes = 4;

}  // namespace

uint16_t crc16(const uint8_t* data, std::size_t len)
{
    // Work in uint32_t: uint16_t operands promote to (signed) int.
    uint32_t crc = 0xFFFFu;
    for (std::size_t i = 0; i < len; ++i)
    {
        crc ^= static_cast<uint32_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b)
        {
            crc = (crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1);
        }
        crc &= 0xFFFFu;
    }
    return static_cast<uint16_t>(crc);
}

std::size_t frame_encode(MsgId msg_id, uint8_t seq, const void* payload, std::size_t payload_len,
    uint8_t* out, std::size_t out_size)
{
    const std::size_t total = kFrameOverhead + payload_len;
    if (payload_len > kFrameMaxPayload || out_size < total)
    {
        return 0;
    }
    out[0] = kFrameSync0;
    out[1] = kFrameSync1;
    out[2] = static_cast<uint8_t>(msg_id);
    out[3] = seq;
    out[4] = static_cast<uint8_t>(payload_len & 0xFFu);
    out[5] = static_cast<uint8_t>(payload_len >> 8);
    if (payload_len > 0)
    {
        std::memcpy(&out[kFrameHeaderSize], payload, payload_len);
    }
    const uint16_t crc = crc16(&out[2], kCrcHeaderBytes + payload_len);
    out[kFrameHeaderSize + payload_len] = static_cast<uint8_t>(crc & 0xFFu);
    out[kFrameHeaderSize + payload_len + 1] = static_cast<uint8_t>(crc >> 8);
    return total;
}

bool FrameParser::feed(uint8_t byte, Frame& out)
{
    // Hunt for the sync bytes.
    if (pos_ == 0)
    {
        if (byte == kFrameSync0)
        {
            buf_[pos_++] = byte;
        }
        return false;
    }
    if (pos_ == 1)
    {
        if (byte == kFrameSync1)
        {
            buf_[pos_++] = byte;
        }
        else
        {
            pos_ = (byte == kFrameSync0) ? 1u : 0u;
        }
        return false;
    }

    buf_[pos_++] = byte;

    if (pos_ < kFrameHeaderSize)
    {
        return false;
    }
    const uint16_t len = static_cast<uint16_t>(buf_[4] | (buf_[5] << 8));
    if (len > kFrameMaxPayload)
    {
        pos_ = 0;  // corrupt length: resynchronize
        return false;
    }
    if (pos_ < kFrameOverhead + len)
    {
        return false;
    }

    const uint16_t rx_crc = static_cast<uint16_t>(
        buf_[kFrameHeaderSize + len] | (buf_[kFrameHeaderSize + len + 1] << 8));
    const uint16_t calc_crc = crc16(&buf_[2], kCrcHeaderBytes + len);
    pos_ = 0;
    if (rx_crc != calc_crc)
    {
        ++crc_errors_;
        return false;
    }

    out.msg_id = static_cast<MsgId>(buf_[2]);
    out.seq = buf_[3];
    out.len = len;
    std::memcpy(out.payload.data(), &buf_[kFrameHeaderSize], len);
    return true;
}

}  // namespace rfs::common
