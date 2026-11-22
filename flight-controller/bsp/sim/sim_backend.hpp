/**
 * Transport between SimBoard and a simulator.
 *
 * Implementations (to be added):
 *   - UdpSimBackend     SITL: firmware on the host, simulator in another process
 *   - SerialSimBackend  HITL: firmware on the real board, simulator on the PC
 * Both carry uav_msg_sim_sensors_t / uav_msg_sim_actuators_t from
 * common/protocol, so the simulator side is the same for SITL and HITL.
 */
#pragma once

#include <cstdint>

#include "uav/protocol/messages.h"

namespace fc::sim
{

class SimBackend
{
public:
    /** Blocks until the simulator sends the next step. false on timeout. */
    virtual bool receive_sensors(uav_msg_sim_sensors_t& out, uint32_t timeout_ms) = 0;

    /** Replies to the step identified by actuators.time_us. */
    virtual void send_actuators(const uav_msg_sim_actuators_t& actuators) = 0;

protected:
    ~SimBackend() = default;
};

}  // namespace fc::sim
