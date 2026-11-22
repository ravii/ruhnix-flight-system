#include "telemetry_task.hpp"

#include <array>

#include "uav/protocol/frame.h"
#include "uav/protocol/messages.h"

namespace app
{

void TelemetryTask::run()
{
    constexpr TickType_t kPeriod = pdMS_TO_TICKS(100);
    std::array<uint8_t, UAV_FRAME_MAX_SIZE> wire{};

    for (;;)
    {
        uav_msg_heartbeat_t hb{};
        hb.node_id = UAV_NODE_FLIGHT_CONTROLLER;
        hb.protocol_version = UAV_PROTOCOL_VERSION;
        hb.uptime_ms = static_cast<uint32_t>(board_.time_us() / 1000u);

        const std::size_t n =
            uav_frame_encode(UAV_MSG_HEARTBEAT, seq_++, &hb, sizeof(hb), wire.data(), wire.size());
        board_.link_write(wire.data(), n);
        board_.led_toggle();
        vTaskDelay(kPeriod);
    }
}

}  // namespace app
