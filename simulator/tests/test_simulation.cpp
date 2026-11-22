#include <cstdio>
#include <vector>

#include "sim/simulation.hpp"

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
class FakeVehicle final : public sim::VehicleModel
{
public:
    std::vector<double> dts;

    void reset(const sim::VehicleState& initial) override { state_ = initial; }
    void step(const uav_msg_sim_actuators_t& act, double dt_s) override
    {
        dts.push_back(dt_s);
        state_.velocity_ned_m_s[2] -= static_cast<double>(act.motors[0]) * dt_s;
    }
    const sim::VehicleState& state() const override { return state_; }

private:
    sim::VehicleState state_;
};

class FakeSensors final : public sim::SensorModel
{
public:
    void sample(
        const sim::VehicleState& truth, uint64_t /*time_us*/, uav_msg_sim_sensors_t& out) override
    {
        out.valid_flags = UAV_SIM_VALID_IMU;
        out.gyro_rad_s[0] = static_cast<float>(truth.angular_velocity_body_rad_s[0]);
    }
};

/** Echoes the step time and commands full motor 0 when RC is valid. */
class FakeLink final : public sim::FcLink
{
public:
    std::vector<uav_msg_sim_sensors_t> received;
    bool answer = true;
    bool wrong_time = false;

    bool exchange(const uav_msg_sim_sensors_t& sensors, uav_msg_sim_actuators_t& act,
        std::chrono::milliseconds /*timeout*/) override
    {
        received.push_back(sensors);
        if (!answer)
        {
            return false;
        }
        act = uav_msg_sim_actuators_t{};
        act.time_us = wrong_time ? sensors.time_us + 1 : sensors.time_us;
        act.motor_count = 4;
        act.motors[0] = (sensors.valid_flags & UAV_SIM_VALID_RC) ? 1.0f : 0.0f;
        return true;
    }
};

void test_lockstep_advances_time_and_physics()
{
    FakeVehicle vehicle;
    FakeSensors sensors;
    FakeLink link;
    sim::Simulation simulation(vehicle, sensors, link);

    sim::Simulation::RcChannels rc{};
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
    CHECK((link.received[1].valid_flags & UAV_SIM_VALID_IMU) != 0);
    CHECK(vehicle.dts.size() == 3 && vehicle.dts[0] == 0.001);
    CHECK(vehicle.state().velocity_ned_m_s[2] < 0.0);  // actuators reached the physics
}

void test_no_reply_does_not_advance()
{
    FakeVehicle vehicle;
    FakeSensors sensors;
    FakeLink link;
    sim::Simulation simulation(vehicle, sensors, link);

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
