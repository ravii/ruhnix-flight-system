#pragma once

#include <cstdint>

#include "fc/bsp/board.hpp"
#include "rtos/task.hpp"

namespace app
{

/** Sends a heartbeat to the companion computer at 10 Hz. */
class TelemetryTask final : public rtos::Task
{
public:
    explicit TelemetryTask(fc::Board& board) : board_(board) {}

private:
    void run() override;

    fc::Board& board_;
    uint8_t seq_ = 0;
};

}  // namespace app
