// Perception service entry point.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include "perception/obstacle_detector.hpp"

namespace
{
std::atomic<bool> g_running{true};
void on_signal(int)
{
    g_running = false;
}
}  // namespace

int main()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    uav::perception::ObstacleDetector detector;
    constexpr auto kPeriod = std::chrono::milliseconds(33);  // ~30 FPS

    std::printf("perception: started\n");
    auto next = std::chrono::steady_clock::now();
    while (g_running)
    {
        // TODO: grab a depth frame from the camera driver (V4L2 / vendor SDK).
        uav::perception::DepthImage frame;
        if (auto obstacle = detector.nearest(frame))
        {
            // TODO: publish to localization / flight controller.
            (void)obstacle;
        }

        next += kPeriod;
        std::this_thread::sleep_until(next);
    }
    std::printf("perception: stopped\n");
    return 0;
}
