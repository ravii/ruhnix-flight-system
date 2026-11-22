/**
 * Transport between SimBoard and a simulator.
 *
 * Implementations (to be added):
 *   - UdpSimBackend     SITL: firmware on the host, simulator in another process
 *   - SerialSimBackend  HITL: firmware on the real board, simulator on the PC
 * Both carry rfs::common::SimSensorsMsg / rfs::common::SimActuatorsMsg from
 * common/protocol, so the simulator side is the same for SITL and HITL.
 */
#pragma once

#include <cstdint>

#include "rfs/common/messages.hpp"

namespace rfs::fc
{

class SimBackend
{
public:
    /** Blocks until the simulator sends the next step. false on timeout. */
    virtual bool receive_sensors(common::SimSensorsMsg& out, uint32_t timeout_ms) = 0;

    /** Replies to the step identified by actuators.time_us. */
    virtual void send_actuators(const common::SimActuatorsMsg& actuators) = 0;

protected:
    ~SimBackend() = default;
};

}  // namespace rfs::fc
