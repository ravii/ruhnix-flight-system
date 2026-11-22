#include "check.hpp"
#include "fc/flight_control.hpp"
#include "fc/mixer.hpp"
#include "fc/pid.hpp"

namespace
{

constexpr float kTol = 1e-5f;

void test_pid_proportional()
{
    fc::Pid pid({2.0f, 0.0f, 0.0f, 1.0f, 10.0f});
    CHECK_NEAR(pid.update(1.0f, 0.25f, 0.001f), 1.5f, kTol);
}

void test_pid_limits()
{
    fc::Pid pid({0.0f, 100.0f, 0.0f, 0.5f, 0.3f});
    for (int i = 0; i < 1000; ++i)
    {
        pid.update(1.0f, 0.0f, 0.01f);
    }
    CHECK_NEAR(pid.integrator(), 0.5f, kTol);               // integrator clamp
    CHECK_NEAR(pid.update(1.0f, 0.0f, 0.01f), 0.3f, kTol);  // output clamp
}

void test_mixer_hover()
{
    const auto m = fc::QuadXMixer{}.mix({0.0f, 0.0f, 0.0f}, 0.5f);
    for (float v : m)
    {
        CHECK_NEAR(v, 0.5f, kTol);
    }
}

void test_mixer_roll_right()
{
    const auto m = fc::QuadXMixer{}.mix({0.1f, 0.0f, 0.0f}, 0.5f);
    // Rolling right: left motors (2, 3) speed up, right motors (0, 1) slow down.
    CHECK(m[2] > m[0] && m[2] > m[1]);
    CHECK(m[3] > m[0] && m[3] > m[1]);
}

void test_mixer_saturation_keeps_attitude()
{
    const auto m = fc::QuadXMixer{}.mix({0.2f, 0.0f, 0.0f}, 1.0f);
    CHECK_NEAR(m[3] - m[0], 0.4f, kTol);  // differential survives at full throttle
    for (float v : m)
    {
        CHECK(v >= 0.0f && v <= 1.0f);
    }
}

fc::FlightControl::Input make_input(bool armed, uint16_t throttle_us)
{
    fc::FlightControl::Input in;
    in.rc.valid = true;
    in.rc.channels.fill(1500);
    in.rc.channels[fc::kRcThrottle] = throttle_us;
    in.rc.channels[fc::kRcArm] = armed ? 2000 : 1000;
    return in;
}

void test_disarmed_outputs_zero()
{
    fc::FlightControl control;
    const fc::ActuatorOutputs out = control.update(make_input(false, 2000), 0.001f);
    CHECK(!out.armed);
    for (float v : out.motors)
    {
        CHECK(v == 0.0f);
    }
}

void test_rc_loss_disarms()
{
    fc::FlightControl control;
    auto in = make_input(true, 1500);
    in.rc.valid = false;
    CHECK(!control.update(in, 0.001f).armed);
}

void test_armed_level_hover_is_symmetric()
{
    fc::FlightControl control;
    const fc::ActuatorOutputs out = control.update(make_input(true, 1500), 0.001f);
    CHECK(out.armed);
    CHECK(out.count == 4);
    for (std::size_t i = 1; i < out.count; ++i)
    {
        CHECK_NEAR(out.motors[i], out.motors[0], kTol);
    }
    CHECK(out.motors[0] > 0.5f);  // idle thrust + half stick
}

void test_rate_error_produces_corrective_torque()
{
    fc::FlightControl control;
    auto in = make_input(true, 1500);
    in.imu.gyro_rad_s[fc::kRoll] = -1.0f;  // rolling left, stick centered
    const fc::ActuatorOutputs out = control.update(in, 0.001f);
    CHECK(control.debug().torque_demand[fc::kRoll] > 0.0f);  // push right
    CHECK(out.motors[3] > out.motors[0]);                    // left side up
}

}  // namespace

int main()
{
    test_pid_proportional();
    test_pid_limits();
    test_mixer_hover();
    test_mixer_roll_right();
    test_mixer_saturation_keeps_attitude();
    test_disarmed_outputs_zero();
    test_rc_loss_disarms();
    test_armed_level_hover_is_symmetric();
    test_rate_error_produces_corrective_torque();
    return TEST_MAIN_RESULT("flight-controller core");
}
