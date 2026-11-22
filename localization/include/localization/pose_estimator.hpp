#pragma once

#include <array>
#include <cstdint>

#include "uav/protocol/messages.h"

namespace uav::localization
{

using Vec3 = std::array<float, 3>;

// Per-axis constant-acceleration Kalman filter on [position, velocity] in world NED.
// Predict with world-frame acceleration (e.g. rotated IMU minus gravity);
// correct with absolute position fixes (GNSS, visual odometry, motion capture).
class PoseEstimator
{
public:
    struct Config
    {
        float accel_noise_var = 0.5f;  // (m/s^2)^2
        float initial_pos_var = 100.0f;
        float initial_vel_var = 10.0f;
    };

    PoseEstimator() : PoseEstimator(Config{}) {}
    explicit PoseEstimator(const Config& config);

    void predict(const Vec3& accel_world_m_s2, float dt_s);
    void correct_position(const Vec3& position_m, float measurement_var);

    const Vec3& position() const { return pos_; }
    const Vec3& velocity() const { return vel_; }
    Vec3 position_variance() const;

    uav_msg_pose_estimate_t to_message(uint64_t timestamp_us) const;

private:
    struct Cov
    {
        float pp, pv, vv;  // symmetric 2x2 covariance
    };

    Config config_;
    Vec3 pos_{};
    Vec3 vel_{};
    std::array<Cov, 3> cov_{};
};

}  // namespace uav::localization
