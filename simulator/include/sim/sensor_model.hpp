/** Sensor model interface: ground truth in, (noisy) sensor readings out. */
#pragma once

#include <cstdint>

#include "sim/vehicle_model.hpp"
#include "uav/protocol/messages.h"

namespace sim
{

/**
 * Fills the sensor fields of a lockstep packet from the true vehicle state.
 * Implementations decide noise, bias, quantization, latency and per-sensor
 * update rates (set UAV_SIM_VALID_* only on steps where a sensor updates).
 * time_us and rc_channels are filled by Simulation.
 */
class SensorModel
{
public:
    virtual ~SensorModel() = default;

    virtual void sample(
        const VehicleState& truth, uint64_t time_us, uav_msg_sim_sensors_t& out) = 0;
};

}  // namespace sim
