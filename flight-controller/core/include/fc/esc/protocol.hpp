/**
 * ESC signal protocols and the raw frame handed to the board.
 *
 * The flight controller drives off-the-shelf ESCs (BLHeli_32, AM32 ...). The
 * protocol is a configuration choice (see config/esc_config.hpp): encoding
 * happens in fc::esc::EscOutput, and the board's fc::EscBus only generates
 * the waveform.
 */
#pragma once

#include <array>
#include <cstdint>

#include "fc/types.hpp"

namespace fc::esc
{

enum class Protocol : uint8_t
{
    kPwm,         ///< analog, 1000..2000 us pulse
    kOneShot125,  ///< analog, 125..250 us pulse
    kDShot150,    ///< digital, 150 kbit/s
    kDShot300,    ///< digital, 300 kbit/s
    kDShot600,    ///< digital, 600 kbit/s
};

constexpr bool is_dshot(Protocol p)
{
    return p == Protocol::kDShot150 || p == Protocol::kDShot300 || p == Protocol::kDShot600;
}

/** DShot bit rate in bit/s; 0 for analog protocols. */
constexpr uint32_t dshot_bitrate(Protocol p)
{
    switch (p)
    {
    case Protocol::kDShot150:
        return 150000;
    case Protocol::kDShot300:
        return 300000;
    case Protocol::kDShot600:
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
constexpr PulseRange analog_pulse_range(Protocol p)
{
    switch (p)
    {
    case Protocol::kPwm:
        return {1000000, 2000000};
    case Protocol::kOneShot125:
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

}  // namespace fc::esc
