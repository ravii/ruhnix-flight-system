#include <array>

#include "check.hpp"
#include "rfs/sim/quadrotor_model.hpp"
#include "rfs/sim/sim_math.hpp"

namespace
{

using rfs::common::SimActuatorsMsg;
using rfs::sim::QuadrotorModel;
using rfs::sim::QuadrotorParams;
using rfs::sim::VehicleState;

constexpr double kDt = 0.001;
constexpr double kG = 9.80665;

SimActuatorsMsg command(const std::array<double, 4>& u, bool armed = true)
{
    SimActuatorsMsg msg{};
    msg.motor_count = 4;
    msg.armed = armed ? 1 : 0;
    for (std::size_t i = 0; i < u.size(); ++i)
    {
        msg.motors[i] = static_cast<float>(u[i]);
    }
    return msg;
}

void run(QuadrotorModel& model, const SimActuatorsMsg& msg, double seconds)
{
    const int steps = static_cast<int>(seconds / kDt + 0.5);
    for (int i = 0; i < steps; ++i)
    {
        model.step(msg, kDt);
    }
}

VehicleState airborne(double altitude_m = 100.0)
{
    VehicleState s;
    s.position_ned_m[2] = -altitude_m;
    return s;
}

void test_rests_on_ground_when_disarmed()
{
    QuadrotorModel model;
    CHECK(model.on_ground());
    CHECK_NEAR(model.state().specific_force_body_m_s2[2], -kG, 1e-9);

    const double h = model.hover_command();
    run(model, command({h, h, h, h}, false), 1.0);  // armed flag off: commands ignored
    CHECK(model.on_ground());
    CHECK(model.state().position_ned_m[2] == 0.0);
    CHECK(model.rotor_speeds()[0] == 0.0);
    CHECK_NEAR(model.state().specific_force_body_m_s2[2], -kG, 1e-9);  // accel reads 1 g up
}

void test_free_fall()
{
    QuadrotorParams params;
    params.linear_drag_n_per_m_s = 0.0;
    QuadrotorModel model(params);
    model.reset(airborne());
    run(model, command({0, 0, 0, 0}), 1.0);
    CHECK(!model.on_ground());
    CHECK_NEAR(model.state().velocity_ned_m_s[2], kG, 1e-9);
    CHECK_NEAR(model.state().position_ned_m[2], -100.0 + 0.5 * kG, 0.01);
    CHECK_NEAR(model.state().specific_force_body_m_s2[2], 0.0, 1e-9);  // accel reads 0 g
}

void test_hover_balances_gravity()
{
    QuadrotorParams params;
    params.linear_drag_n_per_m_s = 0.0;  // the accelerometer would also see drag
    QuadrotorModel model(params);
    model.reset(airborne());
    const double h = model.hover_command();
    run(model, command({h, h, h, h}), 1.0);        // rotors spin up (time constant 20 ms)
    CHECK_NEAR(model.rotor_speeds()[0], h, 1e-6);  // commands travel as float
    CHECK_NEAR(model.state().specific_force_body_m_s2[2], -kG, 1e-4);
    const double vz = model.state().velocity_ned_m_s[2];
    CHECK(vz > 0.0);  // sank while the rotors spun up
    run(model, command({h, h, h, h}), 1.0);
    CHECK_NEAR(model.state().velocity_ned_m_s[2], vz, 1e-3);  // no net force: velocity holds
    for (std::size_t k = 0; k < 3; ++k)
    {
        CHECK_NEAR(model.state().angular_velocity_body_rad_s[k], 0.0, 1e-9);
    }
}

void test_takeoff_from_ground()
{
    QuadrotorModel model;
    run(model, command({0.9, 0.9, 0.9, 0.9}), 1.0);
    CHECK(!model.on_ground());
    CHECK(model.state().position_ned_m[2] < -1.0);  // climbed
}

/** Differential thrust with the rfs::fc::QuadXMixer factors produces torque of the same sign. */
void test_torque_signs_match_mixer()
{
    // {roll, pitch, yaw} factor per motor, copied from flight-controller/core/src/mixer.cpp.
    constexpr std::array<std::array<double, 3>, 4> kMixer = {{
        {-1.0, +1.0, +1.0},
        {-1.0, -1.0, -1.0},
        {+1.0, -1.0, +1.0},
        {+1.0, +1.0, -1.0},
    }};
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        QuadrotorModel model;
        model.reset(airborne());
        const double h = model.hover_command();
        std::array<double, 4> u{};
        for (std::size_t i = 0; i < 4; ++i)
        {
            u[i] = h + 0.05 * kMixer[i][axis];
        }
        run(model, command(u), 0.1);
        const auto& w = model.state().angular_velocity_body_rad_s;
        CHECK(w[axis] > 0.1);
        for (std::size_t other = 0; other < 3; ++other)
        {
            if (other != axis)
            {
                CHECK_NEAR(w[other], 0.0, 1e-6);
            }
        }
    }
}

void test_attitude_integration()
{
    QuadrotorParams params;
    params.linear_drag_n_per_m_s = 0.0;
    QuadrotorModel model(params);
    VehicleState s = airborne();
    s.angular_velocity_body_rad_s = {0.0, 0.0, 1.0};  // yaw right at 1 rad/s
    model.reset(s);
    const double h = model.hover_command();
    run(model, command({h, h, h, h}), 1.0);
    const auto euler = rfs::sim::quat_to_euler(model.state().attitude_wxyz);
    CHECK_NEAR(euler[2], 1.0, 1e-6);
    CHECK_NEAR(euler[0], 0.0, 1e-9);
    CHECK_NEAR(euler[1], 0.0, 1e-9);
}

void test_tilted_thrust_accelerates_sideways()
{
    QuadrotorModel model;
    VehicleState s = airborne();
    s.attitude_wxyz = rfs::sim::quat_from_rotation_vector({0.2, 0.0, 0.0});  // rolled right
    model.reset(s);
    const double h = model.hover_command();
    run(model, command({h, h, h, h}), 0.5);
    CHECK(model.state().velocity_ned_m_s[1] > 0.5);  // moving east (body right)
}

}  // namespace

int main()
{
    test_rests_on_ground_when_disarmed();
    test_free_fall();
    test_hover_balances_gravity();
    test_takeoff_from_ground();
    test_torque_signs_match_mixer();
    test_attitude_integration();
    test_tilted_thrust_accelerates_sideways();
    return TEST_MAIN_RESULT("quadrotor model");
}
