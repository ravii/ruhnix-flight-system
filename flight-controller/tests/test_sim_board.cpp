/**
 * Drives SimBoard + FlightControl the way the firmware's control task does,
 * with an in-memory backend standing in for the simulator.
 */
#include <deque>
#include <vector>

#include "check.hpp"
#include "rfs/fc/flight_control.hpp"
#include "sim_board.hpp"

namespace
{

class FakeBackend final : public rfs::fc::SimBackend
{
public:
    std::deque<rfs::common::SimSensorsMsg> to_board;
    std::vector<rfs::common::SimActuatorsMsg> from_board;

    bool receive_sensors(rfs::common::SimSensorsMsg& out, uint32_t /*timeout_ms*/) override
    {
        if (to_board.empty())
        {
            return false;
        }
        out = to_board.front();
        to_board.pop_front();
        return true;
    }

    void send_actuators(const rfs::common::SimActuatorsMsg& actuators) override
    {
        from_board.push_back(actuators);
    }
};

rfs::common::SimSensorsMsg make_packet(uint64_t time_us, uint8_t flags)
{
    rfs::common::SimSensorsMsg p{};
    p.time_us = time_us;
    p.valid_flags = flags;
    p.gyro_rad_s[0] = 0.1f;
    p.baro_pressure_pa = 101325.0f;
    for (auto& ch : p.rc_channels)
    {
        ch = 1500;
    }
    p.rc_channels[rfs::fc::kRcArm] = 2000;
    return p;
}

void test_lockstep_roundtrip()
{
    FakeBackend backend;
    rfs::fc::SimBoard board(backend);
    rfs::fc::FlightControl control;

    const uint8_t flags = rfs::common::kSimValidImu | rfs::common::kSimValidRc;
    backend.to_board.push_back(make_packet(1000, flags));
    backend.to_board.push_back(make_packet(2000, flags | rfs::common::kSimValidBaro));

    for (int step = 0; step < 2; ++step)
    {
        rfs::fc::FlightControl::Input in;
        CHECK(board.wait_imu(in.imu, 10));
        board.read_rc(in.rc);
        board.write_motors(control.update(in, 0.001f));
    }

    CHECK(board.time_us() == 2000);  // board clock follows simulation time
    CHECK(backend.from_board.size() == 2);
    CHECK(backend.from_board[0].time_us == 1000);  // each reply echoes its step
    CHECK(backend.from_board[1].time_us == 2000);
    CHECK(backend.from_board[1].armed == 1);
    CHECK(backend.from_board[1].motor_count == 4);
}

void test_optional_sensor_read_once_per_packet()
{
    FakeBackend backend;
    rfs::fc::SimBoard board(backend);
    backend.to_board.push_back(
        make_packet(1000, rfs::common::kSimValidImu | rfs::common::kSimValidBaro));

    rfs::fc::ImuSample imu;
    CHECK(board.wait_imu(imu, 10));
    CHECK_NEAR(imu.gyro_rad_s[0], 0.1f, 1e-6f);

    rfs::fc::BaroSample baro;
    CHECK(board.read_baro(baro));
    CHECK_NEAR(baro.pressure_pa, 101325.0f, 1e-3f);
    CHECK(!board.read_baro(baro));  // already consumed

    rfs::fc::MagSample mag;
    CHECK(!board.read_mag(mag));  // not in this packet
}

void test_link_timeout()
{
    FakeBackend backend;
    rfs::fc::SimBoard board(backend);
    rfs::fc::ImuSample imu;
    CHECK(!board.wait_imu(imu, 10));
}

}  // namespace

int main()
{
    test_lockstep_roundtrip();
    test_optional_sensor_read_once_per_packet();
    test_link_timeout();
    return TEST_MAIN_RESULT("sim board");
}
