#include "fc/flight_control.hpp"

#include <algorithm>

namespace fc
{

float rc_to_bipolar(uint16_t pulse_us)
{
    return std::clamp((static_cast<float>(pulse_us) - 1500.0f) / 500.0f, -1.0f, 1.0f);
}

float rc_to_unipolar(uint16_t pulse_us)
{
    return std::clamp((static_cast<float>(pulse_us) - 1000.0f) / 1000.0f, 0.0f, 1.0f);
}

FlightControl::FlightControl(const FlightControlConfig& config)
{
    set_config(config);
}

void FlightControl::set_config(const FlightControlConfig& config)
{
    config_ = config;
    for (std::size_t i = 0; i < rate_pid_.size(); ++i)
    {
        rate_pid_[i].set_gains(config_.rate_gains[i]);
    }
}

void FlightControl::reset()
{
    for (Pid& pid : rate_pid_)
    {
        pid.reset();
    }
    debug_ = Debug{};
}

ActuatorOutputs FlightControl::update(const Input& input, float dt_s)
{
    ActuatorOutputs out;
    out.count = static_cast<uint8_t>(QuadXMixer::kMotorCount);

    const RcInput& rc = input.rc;
    const bool armed = rc.valid && rc.channels[kRcArm] > 1500;
    if (!armed)
    {
        // TODO: proper arming state machine and RC-loss failsafe.
        reset();
        return out;  // motors stay at 0
    }

    const Vec3 stick{rc_to_bipolar(rc.channels[kRcRoll]), rc_to_bipolar(rc.channels[kRcPitch]),
        rc_to_bipolar(rc.channels[kRcYaw])};
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        debug_.rate_setpoint_rad_s[axis] = stick[axis] * config_.max_rate_rad_s[axis];
        debug_.torque_demand[axis] = rate_pid_[axis].update(
            debug_.rate_setpoint_rad_s[axis], input.imu.gyro_rad_s[axis], dt_s);
    }
    const float throttle = rc_to_unipolar(rc.channels[kRcThrottle]);
    debug_.thrust = config_.idle_thrust + (1.0f - config_.idle_thrust) * throttle;

    const QuadXMixer::Output motors = mixer_.mix(debug_.torque_demand, debug_.thrust);
    std::copy(motors.begin(), motors.end(), out.motors.begin());
    out.armed = true;
    return out;
}

}  // namespace fc
