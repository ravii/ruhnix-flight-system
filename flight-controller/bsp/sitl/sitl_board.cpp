/**
 * SITL board: SimBoard (bsp/sim) over UDP, running under the FreeRTOS POSIX port.
 *
 * The firmware is unchanged: app/main.cpp creates the same tasks and the
 * control task blocks in Board::wait_imu(), which here returns when the
 * simulator's next lockstep packet arrives. Board time is simulation time.
 */
#include "FreeRTOS.h"
#include "rfs/fc/bsp/board.hpp"
#include "sim_board.hpp"
#include "task.h"
#include "udp_sim_backend.hpp"

namespace
{

rfs::fc::UdpSimBackend g_backend;
rfs::fc::SimBoard g_board(g_backend);

}  // namespace

rfs::fc::Board& rfs::fc::board()
{
    return g_board;
}

extern "C" void vApplicationIdleHook(void)
{
    g_backend.idle_wait();
}
