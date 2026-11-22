#include "rfs/fc/pid.hpp"

#include <algorithm>

namespace rfs::fc
{

void Pid::reset()
{
    integrator_ = 0.0f;
    prev_measurement_ = 0.0f;
    has_prev_ = false;
}

float Pid::update(float setpoint, float measurement, float dt)
{
    const float error = setpoint - measurement;

    integrator_ = std::clamp(
        integrator_ + gains_.ki * error * dt, -gains_.integrator_limit, gains_.integrator_limit);

    float derivative = 0.0f;
    if (has_prev_ && dt > 0.0f)
    {
        derivative = -(measurement - prev_measurement_) / dt;
    }
    prev_measurement_ = measurement;
    has_prev_ = true;

    const float out = gains_.kp * error + integrator_ + gains_.kd * derivative;
    return std::clamp(out, -gains_.output_limit, gains_.output_limit);
}

}  // namespace rfs::fc
