#include "telemetry_task.hpp"

#include <array>

#include "rfs/common/frame.hpp"
#include "rfs/common/messages.hpp"

namespace rfs::fc
{

void TelemetryTask::run()
{
    constexpr TickType_t kPeriod = pdMS_TO_TICKS(100);
    static_assert(kPeriod * 1000u == 100u * configTICK_RATE_HZ, "period is not a whole tick count");
    std::array<uint8_t, common::kFrameMaxSize> wire{};

    for (;;)
    {
        common::HeartbeatMsg hb{};
        hb.node_id = common::NodeId::kFlightController;
        hb.protocol_version = common::kProtocolVersion;
        hb.uptime_ms = static_cast<uint32_t>(board_.time_us() / 1000u);

        const std::size_t n = common::frame_encode(
            common::MsgId::kHeartbeat, seq_++, &hb, sizeof(hb), wire.data(), wire.size());
        board_.link_write(wire.data(), n);
        board_.led_toggle();
        vTaskDelay(kPeriod);
    }
}

}  // namespace rfs::fc
