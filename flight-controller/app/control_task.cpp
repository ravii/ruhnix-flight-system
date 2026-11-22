#include "control_task.hpp"

#include <algorithm>

namespace rfs::fc
{
namespace
{

constexpr uint32_t kImuTimeoutMs = 10;
constexpr float kMinDt = 1e-4f;  // 10 kHz
constexpr float kMaxDt = 0.01f;  // 100 Hz

}  // namespace

void ControlTask::run()
{
    uint64_t last_us = 0;
    bool has_last = false;  // not last_us != 0: simulation time starts at 0
    for (;;)
    {
        FlightControl::Input in;
        if (!board_.wait_imu(in.imu, kImuTimeoutMs))
        {
            // IMU (or simulator link) lost: cut the motors.
            control_.reset();
            board_.write_motors(ActuatorOutputs{});
            has_last = false;
            continue;
        }
        board_.read_rc(in.rc);

        float dt = kMaxDt;
        if (has_last)
        {
            dt = static_cast<float>(in.imu.timestamp_us - last_us) * 1e-6f;
        }
        last_us = in.imu.timestamp_us;
        has_last = true;

        const ActuatorOutputs out = control_.update(in, std::clamp(dt, kMinDt, kMaxDt));
        board_.write_motors(out);
    }
}

}  // namespace rfs::fc
