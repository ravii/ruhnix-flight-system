/** IMU-only sensor model: ground truth plus optional constant bias and white noise. */
#pragma once

#include <cstdint>
#include <random>

#include "rfs/sim/sensor_model.hpp"

namespace rfs::sim
{

struct ImuNoiseParams
{
    double gyro_noise_std_rad_s = 0.0;  ///< per-sample white noise, 1 sigma
    double accel_noise_std_m_s2 = 0.0;
    Vec3d gyro_bias_rad_s{};
    Vec3d accel_bias_m_s2{};
    uint32_t seed = 1;  ///< fixed seed: runs stay reproducible
};

/**
 * Reports gyro = body angular velocity and accel = specific force on every
 * step (kSimValidImu). Defaults are a perfect IMU. Baro, mag and GNSS are
 * left invalid.
 */
class ImuSensorModel final : public SensorModel
{
public:
    explicit ImuSensorModel(const ImuNoiseParams& params = ImuNoiseParams{});

    void sample(const VehicleState& truth, uint64_t time_us, common::SimSensorsMsg& out) override;

private:
    double noise(double std_dev);

    ImuNoiseParams params_;
    std::mt19937 rng_;
    std::normal_distribution<double> normal_{0.0, 1.0};
};

}  // namespace rfs::sim
