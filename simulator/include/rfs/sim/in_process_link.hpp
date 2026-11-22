/**
 * Algorithm loop: the flight-controller core runs inside the simulator process.
 *
 * InProcessLink feeds each lockstep packet straight into
 * rfs::fc::FlightControl::update() - no RTOS, no board, no serialization - so
 * a whole flight can be single-stepped in a debugger and runs far faster than
 * real time. It mirrors what app/control_task.cpp does on the board: the step
 * dt comes from consecutive IMU timestamps and is clamped the same way, and
 * a packet without IMU data cuts the motors.
 */
#pragma once

#include <cstdint>

#include "rfs/fc/flight_control.hpp"
#include "rfs/sim/fc_link.hpp"

namespace rfs::sim
{

class InProcessLink final : public FcLink
{
public:
    explicit InProcessLink(fc::FlightControl& control) : control_(control) {}

    /** Always answers immediately; timeout is ignored. */
    bool exchange(const common::SimSensorsMsg& sensors, common::SimActuatorsMsg& actuators,
        std::chrono::milliseconds timeout) override;

    fc::FlightControl& control() { return control_; }
    /** The input of the last step, as the flight controller saw it. */
    const fc::FlightControl::Input& last_input() const { return input_; }

private:
    fc::FlightControl& control_;
    fc::FlightControl::Input input_{};
    uint64_t last_imu_us_ = 0;
    bool has_last_imu_ = false;
};

}  // namespace rfs::sim
