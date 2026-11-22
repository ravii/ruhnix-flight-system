#pragma once

#include <cstdint>

#include "rfs/fc/bsp/board.hpp"
#include "rtos/task.hpp"

namespace rfs::fc
{

/** Sends a heartbeat to the companion computer at 10 Hz. */
class TelemetryTask final : public Task
{
public:
    explicit TelemetryTask(Board& board) : board_(board) {}

private:
    void run() override;

    Board& board_;
    uint8_t seq_ = 0;
};

}  // namespace rfs::fc
