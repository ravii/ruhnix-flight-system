#include "localization/pose_estimator.hpp"

namespace uav::localization
{

PoseEstimator::PoseEstimator(const Config& config) : config_(config)
{
    for (auto& c : cov_)
    {
        c = {config_.initial_pos_var, 0.0f, config_.initial_vel_var};
    }
}

void PoseEstimator::predict(const Vec3& accel, float dt)
{
    const float q = config_.accel_noise_var;
    for (std::size_t i = 0; i < 3; ++i)
    {
        pos_[i] += vel_[i] * dt + 0.5f * accel[i] * dt * dt;
        vel_[i] += accel[i] * dt;

        // P = F P F^T + Q, with F = [1 dt; 0 1] and white-acceleration noise Q.
        Cov& c = cov_[i];
        const float pp = c.pp + 2.0f * dt * c.pv + dt * dt * c.vv;
        const float pv = c.pv + dt * c.vv;
        const float vv = c.vv;
        const float dt2 = dt * dt;
        c.pp = pp + q * dt2 * dt2 / 4.0f;
        c.pv = pv + q * dt2 * dt / 2.0f;
        c.vv = vv + q * dt2;
    }
}

void PoseEstimator::correct_position(const Vec3& z, float r)
{
    for (std::size_t i = 0; i < 3; ++i)
    {
        Cov& c = cov_[i];
        const float s = c.pp + r;
        const float k_p = c.pp / s;
        const float k_v = c.pv / s;
        const float innovation = z[i] - pos_[i];

        pos_[i] += k_p * innovation;
        vel_[i] += k_v * innovation;

        // P = (I - K H) P
        const float pp = (1.0f - k_p) * c.pp;
        const float pv = (1.0f - k_p) * c.pv;
        const float vv = c.vv - k_v * c.pv;
        c = {pp, pv, vv};
    }
}

Vec3 PoseEstimator::position_variance() const
{
    return {cov_[0].pp, cov_[1].pp, cov_[2].pp};
}

uav_msg_pose_estimate_t PoseEstimator::to_message(uint64_t timestamp_us) const
{
    uav_msg_pose_estimate_t msg{};
    msg.timestamp_us = timestamp_us;
    for (std::size_t i = 0; i < 3; ++i)
    {
        msg.position_m[i] = pos_[i];
        msg.velocity_m_s[i] = vel_[i];
        msg.position_var[i] = cov_[i].pp;
    }
    msg.quat[0] = 1.0f;  // TODO: attitude estimation
    return msg;
}

}  // namespace uav::localization
