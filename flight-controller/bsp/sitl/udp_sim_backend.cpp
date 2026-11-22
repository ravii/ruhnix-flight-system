#include "udp_sim_backend.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rfs/common/frame.hpp"
#include "rfs/fc/bsp/rtos_time.hpp"

namespace rfs::fc
{
namespace
{

uint16_t port_from_env()
{
    const char* env = std::getenv("RFS_SITL_PORT");
    if (env == nullptr || *env == '\0')
    {
        return UdpSimBackend::kDefaultPort;
    }
    char* end = nullptr;
    const unsigned long port = std::strtoul(env, &end, 10);
    if (*end != '\0' || port == 0 || port > 65535)
    {
        std::fprintf(stderr, "flight_controller: invalid RFS_SITL_PORT '%s'\n", env);
        std::exit(2);
    }
    return static_cast<uint16_t>(port);
}

}  // namespace

void UdpSimBackend::init()
{
    // Runs before the scheduler starts: plain blocking libc calls and stdio are fine here.
    const uint16_t port = port_from_env();
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd_ < 0 || ::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        std::fprintf(stderr, "flight_controller: cannot bind udp 127.0.0.1:%u: %s\n", port,
            std::strerror(errno));
        std::exit(1);
    }
    std::fprintf(
        stderr, "flight_controller (SITL): waiting for the simulator on udp 127.0.0.1:%u\n", port);
}

bool UdpSimBackend::try_receive(common::SimSensorsMsg& out)
{
    std::array<uint8_t, common::kFrameMaxSize> buf{};
    for (;;)
    {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);
        const ssize_t n = ::recvfrom(fd_, buf.data(), buf.size(), MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n < 0)
        {
            return false;  // EAGAIN: nothing queued (other errors: treat the same)
        }

        // One frame per datagram; anything else is dropped.
        common::FrameParser parser;
        common::Frame frame;
        bool complete = false;
        for (ssize_t i = 0; i < n && !complete; ++i)
        {
            complete = parser.feed(buf[static_cast<std::size_t>(i)], frame);
        }
        if (complete && frame.msg_id == common::MsgId::kSimSensors &&
            frame.len == sizeof(common::SimSensorsMsg))
        {
            std::memcpy(&out, frame.payload.data(), sizeof(out));
            peer_ = from;
            has_peer_ = true;
            return true;
        }
    }
}

bool UdpSimBackend::receive_sensors(common::SimSensorsMsg& out, uint32_t timeout_ms)
{
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = timeout_ticks(timeout_ms);
    waiter_ = xTaskGetCurrentTaskHandle();
    for (;;)
    {
        if (try_receive(out))
        {
            waiter_ = nullptr;
            return true;
        }
        const TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout)
        {
            waiter_ = nullptr;
            return false;
        }
        // Woken by idle_wait() when a datagram is queued (stale wake-ups just loop).
        (void)ulTaskNotifyTake(pdTRUE, timeout - elapsed);
    }
}

void UdpSimBackend::send_actuators(const common::SimActuatorsMsg& actuators)
{
    if (!has_peer_)
    {
        return;
    }
    std::array<uint8_t, common::kFrameMaxSize> buf{};
    const std::size_t n = common::frame_encode(common::MsgId::kSimActuators, seq_++, &actuators,
        sizeof(actuators), buf.data(), buf.size());
    // Lossy by design: a dropped reply makes the simulator time out, not the firmware block.
    (void)::sendto(
        fd_, buf.data(), n, MSG_DONTWAIT, reinterpret_cast<const sockaddr*>(&peer_), sizeof(peer_));
}

void UdpSimBackend::idle_wait()
{
    if (fd_ < 0)
    {
        return;
    }
    // Sleep until a datagram arrives. The tick (SIGALRM) interrupts poll()
    // every tick as well, so the scheduler keeps running.
    pollfd p{};
    p.fd = fd_;
    p.events = POLLIN;
    const int ready = ::poll(&p, 1, static_cast<int>(portTICK_PERIOD_MS));
    if (ready > 0 && waiter_ != nullptr)
    {
        xTaskNotifyGive(waiter_);  // preempts the idle task right here
    }
}

}  // namespace rfs::fc
