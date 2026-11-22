/**
 * ESC signal protocols and the raw frame handed to the board.
 *
 * The flight controller drives off-the-shelf ESCs (BLHeli_32, AM32 ...). The
 * protocol is a configuration choice (see config/esc_config.hpp): encoding
 * happens in rfs::fc::EscOutput, and the board's rfs::fc::EscBus only generates
 * the waveform.
 */
#pragma once

#include <array>
#include <cstdint>

#include "rfs/fc/types.hpp"

namespace rfs::fc
{

enum class EscProtocol : uint8_t
{
    kPwm,         ///< analog, 1000..2000 us pulse
    kOneShot125,  ///< analog, 125..250 us pulse
    kDShot150,    ///< digital, 150 kbit/s
    kDShot300,    ///< digital, 300 kbit/s
    kDShot600,    ///< digital, 600 kbit/s
};

constexpr bool is_dshot(EscProtocol p)
{
    return p == EscProtocol::kDShot150 || p == EscProtocol::kDShot300 ||
           p == EscProtocol::kDShot600;
}

/** DShot bit rate in bit/s; 0 for analog protocols. */
constexpr uint32_t dshot_bitrate(EscProtocol p)
{
    switch (p)
    {
    case EscProtocol::kDShot150:
        return 150000;
    case EscProtocol::kDShot300:
        return 300000;
    case EscProtocol::kDShot600:
        return 600000;
    default:
        return 0;
    }
}

/** Pulse width range of an analog protocol, in nanoseconds: stop .. full throttle. */
struct PulseRange
{
    uint32_t min_ns = 0;
    uint32_t max_ns = 0;
};

/** Zero range for DShot protocols. */
constexpr PulseRange analog_pulse_range(EscProtocol p)
{
    switch (p)
    {
    case EscProtocol::kPwm:
        return {1000000, 2000000};
    case EscProtocol::kOneShot125:
        return {125000, 250000};
    default:
        return {};
    }
}

/**
 * Raw values for one transmission; values[i] goes to output channel i.
 *   DShot:   16-bit frame (11-bit value, telemetry bit, 4-bit CRC), see dshot.hpp
 *   analog:  pulse width in nanoseconds
 */
struct EscFrame
{
    std::array<uint32_t, ActuatorOutputs::kMaxMotors> values{};
    uint8_t count = 0;
};

}  // namespace rfs::fc
