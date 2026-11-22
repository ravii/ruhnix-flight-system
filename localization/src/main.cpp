// Localization service entry point.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include "rfs/localization/pose_estimator.hpp"

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

    rfs::localization::PoseEstimator estimator;
    constexpr auto kPeriod = std::chrono::milliseconds(5);  // 200 Hz
    constexpr float kDt = 0.005f;

    std::printf("localization: started\n");
    auto next = std::chrono::steady_clock::now();
    while (g_running)
    {
        // TODO: read IMU from the flight controller, rotate to world frame, remove gravity.
        estimator.predict({0.0f, 0.0f, 0.0f}, kDt);
        // TODO: estimator.correct_position(...) on GNSS / VIO updates.
        // TODO: publish estimator.to_message(...) to perception and the flight controller.

        next += kPeriod;
        std::this_thread::sleep_until(next);
    }
    std::printf("localization: stopped\n");
    return 0;
}
