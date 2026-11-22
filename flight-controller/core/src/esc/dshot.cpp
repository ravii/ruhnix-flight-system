#include "fc/esc/dshot.hpp"

#include <algorithm>
#include <cmath>

namespace fc::esc::dshot
{

uint16_t encode(uint16_t value, bool telemetry, bool bidirectional)
{
    const unsigned packet = (std::min<unsigned>(value, kValueMax) << 1u) | (telemetry ? 1u : 0u);
    unsigned crc = packet ^ (packet >> 4u) ^ (packet >> 8u);
    if (bidirectional)
    {
        crc = ~crc;
    }
    return static_cast<uint16_t>((packet << 4u) | (crc & 0x0Fu));
}

uint16_t throttle_value(float throttle)
{
    if (!(throttle > 0.0f))  // also catches NaN
    {
        return kThrottleMin;
    }
    if (throttle >= 1.0f)
    {
        return kThrottleMax;
    }
    constexpr float kSpan = static_cast<float>(kThrottleMax - kThrottleMin);
    return static_cast<uint16_t>(kThrottleMin + std::lround(throttle * kSpan));
}

uint16_t command_frame(Command command, bool bidirectional)
{
    return encode(static_cast<uint16_t>(command), command != Command::kMotorStop, bidirectional);
}

}  // namespace fc::esc::dshot
