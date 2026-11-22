/**
 * The flight-control algorithm as a single, pure step function.
 *
 * FlightControl never reads a clock, touches hardware or calls the RTOS:
 * everything it needs comes in through Input and everything it produces
 * goes out through ActuatorOutputs. The same object is driven by
 *   - the firmware's control task (app/control_task.cpp) on the real board,
 *   - the host simulator, directly in-process, to debug and tune algorithms,
 *   - unit tests.
 */
#pragma once

#include <array>

#include "rfs/fc/mixer.hpp"
#include "rfs/fc/pid.hpp"
#include "rfs/fc/types.hpp"

namespace rfs::fc
{

struct FlightControlConfig
{
    Vec3 max_rate_rad_s{3.5f, 3.5f, 2.0f};  ///< full-stick body rates (acro mode)
    std::array<PidGains, 3> rate_gains{{
        {0.05f, 0.02f, 0.0005f, 0.2f, 0.5f},  // roll
        {0.05f, 0.02f, 0.0005f, 0.2f, 0.5f},  // pitch
        {0.08f, 0.02f, 0.0f, 0.2f, 0.5f},     // yaw
    }};
    float idle_thrust = 0.05f;  ///< collective thrust while armed at zero stick
};

class FlightControl
{
public:
    struct Input
    {
        ImuSample imu;
        RcInput rc;
    };

    /** Internal signals exposed for logging, plotting and simulator debugging. */
    struct Debug
    {
        Vec3 rate_setpoint_rad_s{};
        Vec3 torque_demand{};
        float thrust = 0.0f;
    };

    explicit FlightControl(const FlightControlConfig& config = FlightControlConfig{});

    void set_config(const FlightControlConfig& config);
    const FlightControlConfig& config() const { return config_; }

    /** Runs one control step. dt_s is the time since the previous step. */
    ActuatorOutputs update(const Input& input, float dt_s);

    void reset();

    const Debug& debug() const { return debug_; }

private:
    FlightControlConfig config_;
    std::array<Pid, 3> rate_pid_;
    QuadXMixer mixer_;
    Debug debug_;
};

/** Maps an RC pulse (1000..2000 us) to [-1, 1]. */
float rc_to_bipolar(uint16_t pulse_us);
/** Maps an RC pulse (1000..2000 us) to [0, 1]. */
float rc_to_unipolar(uint16_t pulse_us);

}  // namespace rfs::fc
