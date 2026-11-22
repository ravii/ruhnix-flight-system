/** Connection from the simulator to the flight controller under test. */
#pragma once

#include <chrono>

#include "rfs/common/messages.hpp"

namespace rfs::sim
{

/**
 * One lockstep exchange: deliver a sensor packet, wait for the actuator reply.
 *
 * Implementations, from fastest to most realistic:
 *   InProcessLink  calls rfs::fc::FlightControl::update() directly (no RTOS).
 *                  For algorithm development, tuning and CI regression runs.
 *                  See in_process_link.hpp.
 *   UdpLink        SITL (udp_link.hpp): full firmware on the host (FreeRTOS POSIX port +
 *                  flight-controller/bsp/sim with a UDP SimBackend).
 *   SerialLink     (to be added) HITL: real flight-controller board over UART.
 */
class FcLink
{
public:
    virtual ~FcLink() = default;

    /** @return false if no reply arrived within timeout. */
    virtual bool exchange(const common::SimSensorsMsg& sensors, common::SimActuatorsMsg& actuators,
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace rfs::sim
