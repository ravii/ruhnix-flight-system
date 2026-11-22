#include "rfs/sim/simulation.hpp"

namespace rfs::sim
{

Simulation::Simulation(
    VehicleModel& vehicle, SensorModel& sensors, FcLink& link, const SimulationConfig& config)
  : vehicle_(vehicle), sensors_(sensors), link_(link), config_(config)
{
    rc_.fill(1500);
}

void Simulation::set_rc(const RcChannels& channels, bool valid)
{
    rc_ = channels;
    rc_valid_ = valid;
}

bool Simulation::step()
{
    common::SimSensorsMsg packet{};
    sensors_.sample(vehicle_.state(), time_us_, packet);
    packet.time_us = time_us_;
    for (std::size_t i = 0; i < rc_.size(); ++i)
    {
        packet.rc_channels[i] = rc_[i];
    }
    if (rc_valid_)
    {
        packet.valid_flags = static_cast<uint8_t>(packet.valid_flags | common::kSimValidRc);
    }

    common::SimActuatorsMsg reply{};
    if (!link_.exchange(packet, reply, config_.link_timeout) || reply.time_us != time_us_)
    {
        return false;  // no answer, or an answer to a different step
    }
    actuators_ = reply;

    vehicle_.step(actuators_, static_cast<double>(config_.step_us) * 1e-6);
    time_us_ += config_.step_us;
    return true;
}

}  // namespace rfs::sim
