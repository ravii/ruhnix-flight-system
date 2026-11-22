#include "rfs/fc/esc/dshot.hpp"

#include <algorithm>
#include <cmath>

namespace rfs::fc
{

uint16_t dshot_encode(uint16_t value, bool telemetry, bool bidirectional)
{
    const unsigned packet =
        (std::min<unsigned>(value, kDshotValueMax) << 1u) | (telemetry ? 1u : 0u);
    unsigned crc = packet ^ (packet >> 4u) ^ (packet >> 8u);
    if (bidirectional)
    {
        crc = ~crc;
    }
    return static_cast<uint16_t>((packet << 4u) | (crc & 0x0Fu));
}

uint16_t dshot_throttle_value(float throttle)
{
    if (!(throttle > 0.0f))  // also catches NaN
    {
        return kDshotThrottleMin;
    }
    if (throttle >= 1.0f)
    {
        return kDshotThrottleMax;
    }
    constexpr float kSpan = static_cast<float>(kDshotThrottleMax - kDshotThrottleMin);
    return static_cast<uint16_t>(kDshotThrottleMin + std::lround(throttle * kSpan));
}

uint16_t dshot_command_frame(DshotCommand command, bool bidirectional)
{
    return dshot_encode(
        static_cast<uint16_t>(command), command != DshotCommand::kMotorStop, bidirectional);
}

}  // namespace rfs::fc
