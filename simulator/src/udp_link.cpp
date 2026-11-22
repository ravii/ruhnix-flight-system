#include "rfs/sim/udp_link.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>

#include "rfs/common/frame.hpp"

namespace rfs::sim
{

UdpLink::~UdpLink()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

bool UdpLink::open(const std::string& host, uint16_t port)
{
    fc_addr_ = sockaddr_in{};
    fc_addr_.sin_family = AF_INET;
    fc_addr_.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &fc_addr_.sin_addr) != 1)
    {
        errno = EINVAL;
        return false;
    }
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);  // ephemeral local port; the firmware replies to it
    return fd_ >= 0;
}

bool UdpLink::exchange(const common::SimSensorsMsg& sensors, common::SimActuatorsMsg& actuators,
    std::chrono::milliseconds timeout)
{
    std::array<uint8_t, common::kFrameMaxSize> buf{};
    const std::size_t n = common::frame_encode(
        common::MsgId::kSimSensors, seq_++, &sensors, sizeof(sensors), buf.data(), buf.size());
    if (::sendto(fd_, buf.data(), n, 0, reinterpret_cast<const sockaddr*>(&fc_addr_),
            sizeof(fc_addr_)) < 0)
    {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;)
    {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0)
        {
            return false;
        }
        pollfd p{};
        p.fd = fd_;
        p.events = POLLIN;
        const int ready = ::poll(&p, 1, static_cast<int>(left.count()));
        if (ready <= 0)
        {
            continue;  // timeout (checked above) or EINTR
        }

        const ssize_t got = ::recv(fd_, buf.data(), buf.size(), 0);
        common::FrameParser parser;
        common::Frame frame;
        bool complete = false;
        for (ssize_t i = 0; i < got && !complete; ++i)
        {
            complete = parser.feed(buf[static_cast<std::size_t>(i)], frame);
        }
        if (!complete || frame.msg_id != common::MsgId::kSimActuators ||
            frame.len != sizeof(common::SimActuatorsMsg))
        {
            ++dropped_;
            continue;
        }
        common::SimActuatorsMsg reply;
        std::memcpy(&reply, frame.payload.data(), sizeof(reply));
        if (reply.time_us != sensors.time_us)
        {
            ++dropped_;  // answers another step
            continue;
        }
        actuators = reply;
        return true;
    }
}

}  // namespace rfs::sim
