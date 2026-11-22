/**
 * ESC configuration: the one place to change when the ESCs or their wiring
 * change. Read by the board (bsp/<board>/board.cpp), which hands it to
 * rfs::fc::EscOutput. The control code only ever sees motor outputs in [0, 1].
 */
#pragma once

#include "rfs/fc/esc/esc_output.hpp"

namespace rfs::fc
{

constexpr EscConfig make_esc_config()
{
    EscConfig c{};
    c.protocol = EscProtocol::kDShot600;
    c.motor_count = 4;
    // Logical motor i (QuadXMixer numbering, see rfs/fc/mixer.hpp) -> output channel.
    // Adjust to the actual wiring instead of re-soldering.
    c.output_map = {{0, 1, 2, 3, 4, 5, 6, 7}};
    c.bidirectional = false;  // eRPM telemetry: enable once the board's EscBus supports it
    return c;
}

inline constexpr EscConfig kEscConfig = make_esc_config();
static_assert(is_valid(kEscConfig), "invalid ESC configuration");

}  // namespace rfs::fc
