/**
 * Lockstep simulation loop.
 *
 * Each step():
 *   1. SensorModel samples the true state at the current simulation time
 *   2. FcLink sends the packet to the flight controller and waits for actuators
 *   3. VehicleModel advances the physics by dt with those actuators
 *   4. simulation time advances by dt
 *
 * The flight controller never sees wall-clock time, so runs are deterministic
 * and can go faster or slower than real time.
 */
#pragma once

#include <array>
#include <chrono>
#include <cstdint>

#include "rfs/sim/fc_link.hpp"
#include "rfs/sim/sensor_model.hpp"
#include "rfs/sim/vehicle_model.hpp"

namespace rfs::sim
{

struct SimulationConfig
{
    uint32_t step_us = 1000;  ///< physics and lockstep period (1 kHz)
    std::chrono::milliseconds link_timeout{1000};
};

class Simulation
{
public:
    using RcChannels = std::array<uint16_t, common::kSimRcChannels>;

    Simulation(VehicleModel& vehicle, SensorModel& sensors, FcLink& link,
        const SimulationConfig& config = SimulationConfig{});

    /** Pilot input for the following steps (from a script, joystick ...). */
    void set_rc(const RcChannels& channels, bool valid = true);

    /** Runs one lockstep step. @return false if the flight controller did not answer. */
    bool step();

    uint64_t time_us() const { return time_us_; }
    const VehicleState& state() const { return vehicle_.state(); }
    const common::SimActuatorsMsg& last_actuators() const { return actuators_; }

private:
    VehicleModel& vehicle_;
    SensorModel& sensors_;
    FcLink& link_;
    SimulationConfig config_;

    uint64_t time_us_ = 0;
    RcChannels rc_{};
    bool rc_valid_ = false;
    common::SimActuatorsMsg actuators_{};
};

}  // namespace rfs::sim
