#pragma once

#include "rfs/fc/bsp/board.hpp"
#include "rfs/fc/flight_control.hpp"
#include "rtos/task.hpp"

namespace rfs::fc
{

/**
 * Rate loop. Paced by Board::wait_imu() (IMU data-ready on hardware, the
 * lockstep sensor packet in simulation), so it never depends on the RTOS tick.
 */
class ControlTask final : public Task
{
public:
    ControlTask(Board& board, FlightControl& control) : board_(board), control_(control) {}

private:
    void run() override;

    Board& board_;
    FlightControl& control_;
};

}  // namespace rfs::fc
