#include "rfs/sim/imu_sensor_model.hpp"

namespace rfs::sim
{

ImuSensorModel::ImuSensorModel(const ImuNoiseParams& params) : params_(params), rng_(params.seed) {}

double ImuSensorModel::noise(double std_dev)
{
    return std_dev > 0.0 ? std_dev * normal_(rng_) : 0.0;
}

void ImuSensorModel::sample(
    const VehicleState& truth, uint64_t /*time_us*/, common::SimSensorsMsg& out)
{
    // Packed message: assign element-wise.
    for (std::size_t k = 0; k < 3; ++k)
    {
        out.gyro_rad_s[k] =
            static_cast<float>(truth.angular_velocity_body_rad_s[k] + params_.gyro_bias_rad_s[k] +
                               noise(params_.gyro_noise_std_rad_s));
        out.accel_m_s2[k] =
            static_cast<float>(truth.specific_force_body_m_s2[k] + params_.accel_bias_m_s2[k] +
                               noise(params_.accel_noise_std_m_s2));
    }
    out.valid_flags = static_cast<uint8_t>(out.valid_flags | common::kSimValidImu);
}

}  // namespace rfs::sim
