/**
 * SITL transport: lockstep packets over UDP, one rfs::common frame per datagram.
 *
 * The firmware binds 127.0.0.1:<port> and answers whoever sent the last
 * sensor packet (rfs_sim --udp 127.0.0.1:<port>).
 *
 * Waiting without starving other tasks: under the FreeRTOS POSIX port a task
 * blocked in a syscall still counts as running, so the control task must not
 * sit in recv(). Instead it blocks on a task notification, and the idle task
 * (only runs when every task is blocked) waits in poll() - the SITL analogue
 * of WFI on the MCU - and notifies the control task when a datagram arrives,
 * like the IMU data-ready interrupt does on a real board.
 */
#pragma once

#include <netinet/in.h>

#include <cstdint>

#include "FreeRTOS.h"
#include "sim_backend.hpp"
#include "task.h"

namespace rfs::fc
{

class UdpSimBackend final : public SimBackend
{
public:
    static constexpr uint16_t kDefaultPort = 14560;

    /** Binds the socket; port from $RFS_SITL_PORT, else kDefaultPort. Exits on failure. */
    void init() override;
    bool receive_sensors(common::SimSensorsMsg& out, uint32_t timeout_ms) override;
    void send_actuators(const common::SimActuatorsMsg& actuators) override;

    /** Called from vApplicationIdleHook(). */
    void idle_wait();

private:
    /** Non-blocking: takes the next valid sensor frame from the socket, if any. */
    bool try_receive(common::SimSensorsMsg& out);

    int fd_ = -1;
    sockaddr_in peer_{};
    bool has_peer_ = false;
    TaskHandle_t waiter_ = nullptr;  ///< task blocked in receive_sensors()
    uint8_t seq_ = 0;
};

}  // namespace rfs::fc
