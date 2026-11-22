/**
 * rfs::fc::Board implementation backed by a simulator.
 *
 * Each wait_imu() consumes one lockstep sensor packet; the packet's time
 * becomes the board clock, so the firmware runs on simulation time.
 * write_motors() answers that packet. The other sensors report a new sample
 * once per packet that flags them as valid.
 */
#pragma once

#include "rfs/fc/bsp/board.hpp"
#include "sim_backend.hpp"

namespace rfs::fc
{

class SimBoard final : public Board
{
public:
    explicit SimBoard(SimBackend& backend) : backend_(backend) {}

    void init() override {}
    uint64_t time_us() override { return packet_.time_us; }
    bool wait_imu(ImuSample& out, uint32_t timeout_ms) override;
    bool read_baro(BaroSample& out) override;
    bool read_mag(MagSample& out) override;
    bool read_gnss(GnssSample& out) override;
    void read_rc(RcInput& out) override;
    void write_motors(const ActuatorOutputs& outputs) override;

    // TODO: route to a UDP socket so perception/localization can connect in SITL.
    std::size_t link_write(const uint8_t* /*data*/, std::size_t len) override { return len; }
    std::size_t link_read(uint8_t* /*data*/, std::size_t /*max_len*/) override { return 0; }

private:
    bool take(uint8_t flag);

    SimBackend& backend_;
    common::SimSensorsMsg packet_{};
    uint8_t unread_ = 0;  ///< rfs::common::kSimValid* bits not yet consumed
};

}  // namespace rfs::fc
