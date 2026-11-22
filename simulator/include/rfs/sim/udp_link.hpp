/**
 * SITL link: lockstep exchange with the firmware (bsp/sitl) over UDP.
 *
 * One rfs::common frame per datagram: kSimSensors to the firmware,
 * kSimActuators back. Replies whose time_us does not match the request (late
 * answers, or the firmware's motor-off reply after its own IMU timeout) are
 * discarded. Linux/POSIX only.
 */
#pragma once

#include <netinet/in.h>

#include <cstdint>
#include <string>

#include "rfs/sim/fc_link.hpp"

namespace rfs::sim
{

class UdpLink final : public FcLink
{
public:
    UdpLink() = default;
    ~UdpLink() override;
    UdpLink(const UdpLink&) = delete;
    UdpLink& operator=(const UdpLink&) = delete;

    /** host is a dotted IPv4 address. @return false (with errno set) on failure. */
    bool open(const std::string& host, uint16_t port);

    bool exchange(const common::SimSensorsMsg& sensors, common::SimActuatorsMsg& actuators,
        std::chrono::milliseconds timeout) override;

    /** Replies dropped because they answered another step or were malformed. */
    uint32_t dropped() const { return dropped_; }

private:
    int fd_ = -1;
    uint8_t seq_ = 0;
    uint32_t dropped_ = 0;
    sockaddr_in fc_addr_{};
};

}  // namespace rfs::sim
