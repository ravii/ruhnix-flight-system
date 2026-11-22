/** Sensor model interface: ground truth in, (noisy) sensor readings out. */
#pragma once

#include <cstdint>

#include "rfs/common/messages.hpp"
#include "rfs/sim/vehicle_model.hpp"

namespace rfs::sim
{

/**
 * Fills the sensor fields of a lockstep packet from the true vehicle state.
 * Implementations decide noise, bias, quantization, latency and per-sensor
 * update rates (set rfs::common::kSimValid* only on steps where a sensor updates).
 * time_us and rc_channels are filled by Simulation.
 */
class SensorModel
{
public:
    virtual ~SensorModel() = default;

    virtual void sample(
        const VehicleState& truth, uint64_t time_us, common::SimSensorsMsg& out) = 0;
};

}  // namespace rfs::sim
