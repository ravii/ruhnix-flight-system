#include <cstdio>
#include <vector>

#include "rfs/sim/simulation.hpp"

namespace
{

int failures = 0;

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

/** Integrates thrust along -z (NED up) so steps are observable. */
class FakeVehicle final : public rfs::sim::VehicleModel
{
public:
    std::vector<double> dts;

    void reset(const rfs::sim::VehicleState& initial) override { state_ = initial; }
    void step(const rfs::common::SimActuatorsMsg& act, double dt_s) override
    {
        dts.push_back(dt_s);
        state_.velocity_ned_m_s[2] -= static_cast<double>(act.motors[0]) * dt_s;
    }
    const rfs::sim::VehicleState& state() const override { return state_; }

private:
    rfs::sim::VehicleState state_;
};

class FakeSensors final : public rfs::sim::SensorModel
{
public:
    void sample(const rfs::sim::VehicleState& truth, uint64_t /*time_us*/,
        rfs::common::SimSensorsMsg& out) override
    {
        out.valid_flags = rfs::common::kSimValidImu;
        out.gyro_rad_s[0] = static_cast<float>(truth.angular_velocity_body_rad_s[0]);
    }
};

/** Echoes the step time and commands full motor 0 when RC is valid. */
class FakeLink final : public rfs::sim::FcLink
{
public:
    std::vector<rfs::common::SimSensorsMsg> received;
    bool answer = true;
    bool wrong_time = false;

    bool exchange(const rfs::common::SimSensorsMsg& sensors, rfs::common::SimActuatorsMsg& act,
        std::chrono::milliseconds /*timeout*/) override
    {
        received.push_back(sensors);
        if (!answer)
        {
            return false;
        }
        act = rfs::common::SimActuatorsMsg{};
        act.time_us = wrong_time ? sensors.time_us + 1 : sensors.time_us;
        act.motor_count = 4;
        act.motors[0] = (sensors.valid_flags & rfs::common::kSimValidRc) ? 1.0f : 0.0f;
        return true;
    }
};

void test_lockstep_advances_time_and_physics()
{
    FakeVehicle vehicle;
    FakeSensors sensors;
    FakeLink link;
    rfs::sim::Simulation simulation(vehicle, sensors, link);

    rfs::sim::Simulation::RcChannels rc{};
    rc.fill(1500);
    simulation.set_rc(rc);

    for (int i = 0; i < 3; ++i)
    {
        CHECK(simulation.step());
    }
    CHECK(simulation.time_us() == 3000);
    CHECK(link.received.size() == 3);
    CHECK(link.received[0].time_us == 0);
    CHECK(link.received[2].time_us == 2000);
    CHECK(link.received[1].rc_channels[0] == 1500);
    CHECK((link.received[1].valid_flags & rfs::common::kSimValidImu) != 0);
    CHECK(vehicle.dts.size() == 3 && vehicle.dts[0] == 0.001);
    CHECK(vehicle.state().velocity_ned_m_s[2] < 0.0);  // actuators reached the physics
}

void test_no_reply_does_not_advance()
{
    FakeVehicle vehicle;
    FakeSensors sensors;
    FakeLink link;
    rfs::sim::Simulation simulation(vehicle, sensors, link);

    link.answer = false;
    CHECK(!simulation.step());
    CHECK(simulation.time_us() == 0);
    CHECK(vehicle.dts.empty());

    link.answer = true;
    link.wrong_time = true;  // stale or out-of-order reply
    CHECK(!simulation.step());
    CHECK(vehicle.dts.empty());
}

}  // namespace

int main()
{
    test_lockstep_advances_time_and_physics();
    test_no_reply_does_not_advance();
    if (failures)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all simulator tests passed\n");
    return 0;
}
