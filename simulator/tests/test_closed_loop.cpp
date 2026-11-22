/**
 * Algorithm loop regression tests: rfs::fc::FlightControl flying the built-in
 * quadrotor model through rfs::sim::Simulation and InProcessLink.
 */
#include <cmath>

#include "check.hpp"
#include "rfs/fc/flight_control.hpp"
#include "rfs/sim/imu_sensor_model.hpp"
#include "rfs/sim/in_process_link.hpp"
#include "rfs/sim/quadrotor_model.hpp"
#include "rfs/sim/sim_math.hpp"
#include "rfs/sim/simulation.hpp"

namespace
{

using rfs::sim::Simulation;

/** Model, sensors, flight controller and lockstep loop wired together. */
struct Rig
{
    explicit Rig(const rfs::sim::ImuNoiseParams& noise = rfs::sim::ImuNoiseParams{})
      : sensors(noise), link(control), sim(model, sensors, link)
    {}

    rfs::sim::QuadrotorModel model;
    rfs::sim::ImuSensorModel sensors;
    rfs::fc::FlightControl control;
    rfs::sim::InProcessLink link;
    Simulation sim;

    void start_airborne(double altitude_m = 100.0)
    {
        rfs::sim::VehicleState s;
        s.position_ned_m[2] = -altitude_m;
        model.reset(s);
    }

    /** RC pulse for the throttle stick that commands hover thrust. */
    uint16_t hover_throttle_us() const
    {
        const double idle = static_cast<double>(control.config().idle_thrust);
        const double stick = (model.hover_command() - idle) / (1.0 - idle);
        return static_cast<uint16_t>(std::lround(1000.0 + 1000.0 * stick));
    }

    void set_sticks(bool armed, uint16_t throttle_us, uint16_t roll_us = 1500,
        uint16_t pitch_us = 1500, uint16_t yaw_us = 1500)
    {
        Simulation::RcChannels rc{};
        rc.fill(1500);
        rc[rfs::fc::kRcRoll] = roll_us;
        rc[rfs::fc::kRcPitch] = pitch_us;
        rc[rfs::fc::kRcThrottle] = throttle_us;
        rc[rfs::fc::kRcYaw] = yaw_us;
        rc[rfs::fc::kRcArm] = armed ? 2000 : 1000;
        sim.set_rc(rc);
    }

    bool run(double seconds)
    {
        const int steps = static_cast<int>(seconds * 1000.0 + 0.5);  // 1 kHz default step
        for (int i = 0; i < steps; ++i)
        {
            if (!sim.step())
            {
                return false;
            }
        }
        return true;
    }

    const rfs::sim::Vec3d& rates() const { return sim.state().angular_velocity_body_rad_s; }
};

void test_disarmed_stays_on_ground()
{
    Rig rig;
    rig.set_sticks(false, 2000);  // full throttle, but disarmed
    CHECK(rig.run(1.0));
    CHECK(rig.model.on_ground());
    CHECK(rig.sim.last_actuators().armed == 0);
    CHECK(rig.sim.last_actuators().motors[0] == 0.0f);
    CHECK(rig.sim.time_us() == 1000000);
}

void test_armed_idle_stays_on_ground_then_takes_off()
{
    Rig rig;
    rig.set_sticks(true, 1000);  // armed, zero throttle: idle thrust only
    CHECK(rig.run(1.0));
    CHECK(rig.model.on_ground());
    CHECK(rig.sim.last_actuators().armed == 1);
    CHECK(rig.model.rotor_speeds()[0] > 0.0);  // props spin at idle

    rig.set_sticks(true, 1800);
    CHECK(rig.run(1.0));
    CHECK(!rig.model.on_ground());
    CHECK(rig.sim.state().position_ned_m[2] < -1.0);
    // Level climb: the rate loop holds the attitude.
    const auto euler = rfs::sim::quat_to_euler(rig.sim.state().attitude_wxyz);
    CHECK_NEAR(euler[0], 0.0, 0.01);
    CHECK_NEAR(euler[1], 0.0, 0.01);
}

void test_hover_throttle_holds_altitude()
{
    Rig rig;
    rig.start_airborne();
    rig.set_sticks(true, rig.hover_throttle_us());
    CHECK(rig.run(3.0));
    CHECK_NEAR(rig.sim.state().velocity_ned_m_s[2], 0.0, 0.3);  // only the spin-up dip
}

/**
 * Half stick on one axis: the body rate converges to half the configured
 * maximum, with the sign defined by the stick, and the other axes stay quiet.
 * Catches sign errors anywhere between RC mapping, PID, mixer and motor layout.
 */
void check_rate_step(std::size_t axis, double settle_s)
{
    Rig rig;
    rig.start_airborne();
    const uint16_t hover = rig.hover_throttle_us();
    rig.set_sticks(true, hover);
    CHECK(rig.run(0.2));

    const uint16_t roll = axis == rfs::fc::kRoll ? 1750 : 1500;
    const uint16_t pitch = axis == rfs::fc::kPitch ? 1750 : 1500;
    const uint16_t yaw = axis == rfs::fc::kYaw ? 1750 : 1500;
    rig.set_sticks(true, hover, roll, pitch, yaw);
    CHECK(rig.run(settle_s));

    const double target = 0.5 * static_cast<double>(rig.control.config().max_rate_rad_s[axis]);
    CHECK_NEAR(rig.rates()[axis], target, 0.05 * target);
    for (std::size_t other = 0; other < 3; ++other)
    {
        if (other != axis)
        {
            CHECK_NEAR(rig.rates()[other], 0.0, 0.05);
        }
    }

    // Sticks centered again: the rotation stops.
    rig.set_sticks(true, hover);
    CHECK(rig.run(settle_s));
    CHECK_NEAR(rig.rates()[axis], 0.0, 0.05 * target);
}

void test_rate_steps()
{
    check_rate_step(rfs::fc::kRoll, 0.5);
    check_rate_step(rfs::fc::kPitch, 0.5);
    check_rate_step(rfs::fc::kYaw, 1.5);  // less control authority in yaw
}

void test_imu_loss_cuts_motors()
{
    /** Drops IMU data, like a dead sensor or a lost SITL link. */
    class NoImu final : public rfs::sim::SensorModel
    {
    public:
        void sample(const rfs::sim::VehicleState&, uint64_t, rfs::common::SimSensorsMsg&) override
        {}
    };
    NoImu sensors;
    rfs::sim::QuadrotorModel model;
    rfs::fc::FlightControl control;
    rfs::sim::InProcessLink link(control);
    Simulation sim(model, sensors, link);
    Simulation::RcChannels rc{};
    rc.fill(2000);  // armed, full throttle
    sim.set_rc(rc);
    CHECK(sim.step());
    CHECK(sim.last_actuators().armed == 0);
    CHECK(sim.last_actuators().motors[0] == 0.0f);
}

/** Same seed, same noise, same flight: the loop is deterministic. */
void test_noisy_runs_are_reproducible()
{
    rfs::sim::ImuNoiseParams noise;
    noise.gyro_noise_std_rad_s = 0.01;
    noise.accel_noise_std_m_s2 = 0.1;
    noise.seed = 42;

    Rig a(noise);
    Rig b(noise);
    for (Rig* rig : {&a, &b})
    {
        rig->start_airborne();
        rig->set_sticks(true, rig->hover_throttle_us(), 1600);
        CHECK(rig->run(1.0));
    }
    for (std::size_t k = 0; k < 4; ++k)
    {
        CHECK(a.sim.state().attitude_wxyz[k] == b.sim.state().attitude_wxyz[k]);
    }
    CHECK(a.sim.state().position_ned_m[2] == b.sim.state().position_ned_m[2]);
}

}  // namespace

int main()
{
    test_disarmed_stays_on_ground();
    test_armed_idle_stays_on_ground_then_takes_off();
    test_hover_throttle_holds_altitude();
    test_rate_steps();
    test_imu_loss_cuts_motors();
    test_noisy_runs_are_reproducible();
    return TEST_MAIN_RESULT("closed-loop");
}
