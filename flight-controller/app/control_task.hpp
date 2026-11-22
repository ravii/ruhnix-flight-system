#pragma once

#include "fc/bsp/board.hpp"
#include "fc/flight_control.hpp"
#include "rtos/task.hpp"

namespace app
{

/**
 * Rate loop. Paced by Board::wait_imu() (IMU data-ready on hardware, the
 * lockstep sensor packet in simulation), so it never depends on the RTOS tick.
 */
class ControlTask final : public rtos::Task
{
public:
    ControlTask(fc::Board& board, fc::FlightControl& control) : board_(board), control_(control) {}

private:
    void run() override;

    fc::Board& board_;
    fc::FlightControl& control_;
};

}  // namespace app
