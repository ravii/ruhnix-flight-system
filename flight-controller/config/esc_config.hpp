/**
 * ESC configuration: the one place to change when the ESCs or their wiring
 * change. Read by the board (bsp/<board>/board.cpp), which hands it to
 * fc::esc::EscOutput. The control code only ever sees motor outputs in [0, 1].
 */
#pragma once

#include "fc/esc/esc_output.hpp"

namespace fc::config
{

constexpr esc::EscConfig make_esc_config()
{
    esc::EscConfig c{};
    c.protocol = esc::Protocol::kDShot600;
    c.motor_count = 4;
    // Logical motor i (QuadXMixer numbering, see fc/mixer.hpp) -> output channel.
    // Adjust to the actual wiring instead of re-soldering.
    c.output_map = {{0, 1, 2, 3, 4, 5, 6, 7}};
    c.bidirectional = false;  // eRPM telemetry: enable once the board's EscBus supports it
    return c;
}

inline constexpr esc::EscConfig kEsc = make_esc_config();
static_assert(esc::is_valid(kEsc), "invalid ESC configuration");

}  // namespace fc::config
