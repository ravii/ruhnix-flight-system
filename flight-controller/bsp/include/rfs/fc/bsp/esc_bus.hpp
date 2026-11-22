/**
 * Transport for ESC signals. Each real board implements it (timers + DMA).
 *
 * EscBus only moves raw values: protocol choice and encoding live in
 * rfs::fc::EscOutput (core/), so switching ESC protocol never changes board
 * code beyond supporting that waveform.
 */
#pragma once

#include <array>
#include <cstdint>

#include "rfs/fc/esc/protocol.hpp"
#include "rfs/fc/types.hpp"

namespace rfs::fc
{

class EscBus
{
public:
    /**
     * Sets up output channels 0..channel_count-1 for the protocol.
     * @return false if the board cannot generate it (e.g. no DMA on a pin).
     */
    virtual bool configure(EscProtocol protocol, bool bidirectional, uint8_t channel_count) = 0;

    /**
     * Starts transmitting frame.values[i] on channel i (see EscFrame for
     * the meaning of the values). Non-blocking; called once per control step.
     */
    virtual void send(const EscFrame& frame) = 0;

    /**
     * Bidirectional DShot: latest eRPM per channel.
     * @return false if unsupported or no telemetry received yet.
     */
    virtual bool read_erpm(std::array<uint32_t, ActuatorOutputs::kMaxMotors>& /*out*/)
    {
        return false;
    }

protected:
    // Bus objects are static members of a board, never deleted through this interface.
    ~EscBus() = default;
};

}  // namespace rfs::fc
