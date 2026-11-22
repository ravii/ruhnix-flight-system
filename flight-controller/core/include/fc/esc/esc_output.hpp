/**
 * Protocol layer between the flight-control output and the ESCs.
 *
 *   FlightControl ──ActuatorOutputs (0..1)──▶ EscOutput ──EscFrame──▶ fc::EscBus (board)
 *
 * EscOutput maps logical motors (mixer numbering) to output channels and
 * encodes each throttle for the configured protocol. It has no hardware or
 * RTOS dependency and is unit-tested on the host.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fc/bsp/esc_bus.hpp"
#include "fc/esc/dshot.hpp"
#include "fc/esc/protocol.hpp"
#include "fc/types.hpp"

namespace fc::esc
{

struct EscConfig
{
    static constexpr std::size_t kMaxMotors = ActuatorOutputs::kMaxMotors;

    Protocol protocol = Protocol::kDShot600;
    uint8_t motor_count = 4;
    /** output_map[i] = output channel that drives logical motor i (mixer numbering). */
    std::array<uint8_t, kMaxMotors> output_map{{0, 1, 2, 3, 4, 5, 6, 7}};
    /** eRPM telemetry on the signal wire. DShot only; also needs board support. */
    bool bidirectional = false;
};

/**
 * motor_count in 1..kMaxMotors, output_map[0..motor_count) a permutation of
 * 0..motor_count-1, and bidirectional only with DShot.
 */
constexpr bool is_valid(const EscConfig& config)
{
    if (config.motor_count == 0 || config.motor_count > EscConfig::kMaxMotors)
    {
        return false;
    }
    if (config.bidirectional && !is_dshot(config.protocol))
    {
        return false;
    }
    std::array<bool, EscConfig::kMaxMotors> used{};
    for (std::size_t i = 0; i < config.motor_count; ++i)
    {
        const uint8_t channel = config.output_map[i];
        if (channel >= config.motor_count || used[channel])
        {
            return false;
        }
        used[channel] = true;
    }
    return true;
}

class EscOutput
{
public:
    using Erpm = std::array<uint32_t, EscConfig::kMaxMotors>;

    explicit EscOutput(EscBus& bus) : bus_(bus) {}

    /**
     * Validates the config and configures the bus. Until this succeeds,
     * write() and send_command() send nothing.
     */
    bool init(const EscConfig& config);

    /**
     * Sends one frame. Disarmed: every motor gets "stop" (DShot 0 / minimum
     * pulse). Armed: throttle 0 is armed idle (DShot 48), so the motors spin.
     * Motors at index >= outputs.count are stopped.
     */
    void write(const ActuatorOutputs& outputs);

    /**
     * Sends one DShot command frame to the motors in motor_mask (bit i =
     * logical motor i); the others get motor stop. Use only while disarmed
     * and repeat dshot::kCommandRepeat times for settings commands.
     * @return false if not initialized or the protocol is not DShot.
     */
    bool send_command(dshot::Command command, uint8_t motor_mask);

    /** Latest eRPM per logical motor (bidirectional DShot). */
    bool read_erpm(Erpm& out);

    /** The frame write() would send, without bus I/O. Empty if not initialized. */
    EscFrame encode(const ActuatorOutputs& outputs) const;

    bool initialized() const { return initialized_; }
    const EscConfig& config() const { return config_; }

private:
    EscBus& bus_;
    EscConfig config_{};
    bool initialized_ = false;
};

}  // namespace fc::esc
