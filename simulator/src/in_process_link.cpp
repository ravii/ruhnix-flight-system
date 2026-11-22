#include "rfs/sim/in_process_link.hpp"

#include <algorithm>

namespace rfs::sim
{
namespace
{

// Same limits as flight-controller/app/control_task.cpp.
constexpr float kMinDt = 1e-4f;  // 10 kHz
constexpr float kMaxDt = 0.01f;  // 100 Hz

}  // namespace

bool InProcessLink::exchange(const common::SimSensorsMsg& sensors,
    common::SimActuatorsMsg& actuators, std::chrono::milliseconds /*timeout*/)
{
    actuators = common::SimActuatorsMsg{};
    actuators.time_us = sensors.time_us;

    fc::ActuatorOutputs out{};
    if ((sensors.valid_flags & common::kSimValidImu) == 0)
    {
        // Like an IMU timeout on the board: reset and cut the motors.
        control_.reset();
        has_last_imu_ = false;
    }
    else
    {
        // Packed message: copy element-wise.
        input_.imu.timestamp_us = sensors.time_us;
        for (std::size_t k = 0; k < 3; ++k)
        {
            input_.imu.gyro_rad_s[k] = sensors.gyro_rad_s[k];
            input_.imu.accel_m_s2[k] = sensors.accel_m_s2[k];
        }
        input_.rc.timestamp_us = sensors.time_us;
        input_.rc.valid = (sensors.valid_flags & common::kSimValidRc) != 0;
        static_assert(
            fc::RcInput::kChannels == common::kSimRcChannels, "RC channel count mismatch");
        for (std::size_t i = 0; i < fc::RcInput::kChannels; ++i)
        {
            input_.rc.channels[i] = sensors.rc_channels[i];
        }

        float dt = kMaxDt;
        if (has_last_imu_)
        {
            dt = static_cast<float>(sensors.time_us - last_imu_us_) * 1e-6f;
        }
        last_imu_us_ = sensors.time_us;
        has_last_imu_ = true;

        out = control_.update(input_, std::clamp(dt, kMinDt, kMaxDt));
    }

    static_assert(fc::ActuatorOutputs::kMaxMotors == common::kMaxMotors, "motor count mismatch");
    actuators.motor_count = out.count;
    actuators.armed = out.armed ? 1 : 0;
    for (std::size_t i = 0; i < common::kMaxMotors; ++i)
    {
        actuators.motors[i] = out.motors[i];
    }
    return true;
}

}  // namespace rfs::sim
