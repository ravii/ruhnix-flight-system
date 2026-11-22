#include "check.hpp"
#include "esc_config.hpp"
#include "fc/esc/dshot.hpp"
#include "fc/esc/esc_output.hpp"

namespace
{

namespace dshot = fc::esc::dshot;
using fc::esc::EscConfig;
using fc::esc::EscOutput;
using fc::esc::Protocol;

/** Records what EscOutput hands to the board. */
class FakeEscBus final : public fc::EscBus
{
public:
    bool configure(Protocol p, bool bidir, uint8_t count) override
    {
        configured = true;
        protocol = p;
        bidirectional = bidir;
        channel_count = count;
        return accept_configure;
    }

    void send(const fc::esc::EscFrame& frame) override
    {
        last = frame;
        ++sends;
    }

    bool read_erpm(std::array<uint32_t, fc::ActuatorOutputs::kMaxMotors>& out) override
    {
        out = erpm;
        return has_erpm;
    }

    bool accept_configure = true;
    bool configured = false;
    Protocol protocol = Protocol::kPwm;
    bool bidirectional = false;
    uint8_t channel_count = 0;
    fc::esc::EscFrame last{};
    int sends = 0;
    std::array<uint32_t, fc::ActuatorOutputs::kMaxMotors> erpm{};
    bool has_erpm = false;
};

fc::ActuatorOutputs armed_outputs(float a, float b, float c, float d)
{
    fc::ActuatorOutputs out;
    out.count = 4;
    out.armed = true;
    out.motors[0] = a;
    out.motors[1] = b;
    out.motors[2] = c;
    out.motors[3] = d;
    return out;
}

EscConfig config_with(Protocol protocol)
{
    EscConfig c{};
    c.protocol = protocol;
    return c;
}

// ---- DShot encoding ---------------------------------------------------------

void test_dshot_reference_frame()
{
    // Value 1046 without telemetry: packet 0x82C, CRC 0x82C ^ 0x82 ^ 0x8 = 0x..6.
    CHECK(dshot::encode(1046, false, false) == 0x82C6);
    CHECK(dshot::encode(1046, true, false) == 0x82D7);
    // Bidirectional DShot inverts the CRC.
    CHECK(dshot::encode(1046, false, true) == 0x82C9);
}

void test_dshot_value_is_clamped()
{
    CHECK(dshot::encode(5000, false, false) == dshot::encode(dshot::kValueMax, false, false));
}

void test_dshot_throttle_mapping()
{
    CHECK(dshot::throttle_value(0.0f) == dshot::kThrottleMin);
    CHECK(dshot::throttle_value(1.0f) == dshot::kThrottleMax);
    CHECK(dshot::throttle_value(0.5f) == 1048);  // 48 + round(0.5 * 1999)
    CHECK(dshot::throttle_value(-1.0f) == dshot::kThrottleMin);
    CHECK(dshot::throttle_value(2.0f) == dshot::kThrottleMax);
    CHECK(dshot::throttle_value(std::nanf("")) == dshot::kThrottleMin);
}

void test_dshot_commands()
{
    CHECK(dshot::command_frame(dshot::Command::kMotorStop, false) == 0x0000);
    CHECK(dshot::command_frame(dshot::Command::kMotorStop, true) == 0x000F);
    // Beep1: value 1 with the telemetry bit -> packet 0x003, CRC 0x3.
    CHECK(dshot::command_frame(dshot::Command::kBeep1, false) == 0x0033);
}

// ---- configuration ----------------------------------------------------------

void test_config_validation()
{
    CHECK(fc::esc::is_valid(EscConfig{}));

    EscConfig c{};
    c.motor_count = 0;
    CHECK(!fc::esc::is_valid(c));

    c = EscConfig{};
    c.motor_count = 9;
    CHECK(!fc::esc::is_valid(c));

    c = EscConfig{};
    c.output_map = {{0, 0, 2, 3, 4, 5, 6, 7}};  // channel 0 used twice
    CHECK(!fc::esc::is_valid(c));

    c = EscConfig{};
    c.output_map = {{0, 1, 2, 4, 3, 5, 6, 7}};  // channel 4 with only 4 motors
    CHECK(!fc::esc::is_valid(c));

    c = config_with(Protocol::kPwm);
    c.bidirectional = true;
    CHECK(!fc::esc::is_valid(c));

    CHECK(fc::esc::is_valid(fc::config::kEsc));
}

// ---- EscOutput --------------------------------------------------------------

void test_init_configures_bus()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    EscConfig c = config_with(Protocol::kDShot300);
    c.bidirectional = true;
    CHECK(esc.init(c));
    CHECK(bus.configured);
    CHECK(bus.protocol == Protocol::kDShot300);
    CHECK(bus.bidirectional);
    CHECK(bus.channel_count == 4);
}

void test_invalid_config_or_bus_refusal_sends_nothing()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    EscConfig bad{};
    bad.motor_count = 0;
    CHECK(!esc.init(bad));
    CHECK(!bus.configured);

    bus.accept_configure = false;
    CHECK(!esc.init(EscConfig{}));
    esc.write(armed_outputs(1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(bus.sends == 0);
}

void test_dshot_disarmed_sends_motor_stop()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    CHECK(esc.init(EscConfig{}));
    fc::ActuatorOutputs out = armed_outputs(0.7f, 0.7f, 0.7f, 0.7f);
    out.armed = false;
    esc.write(out);
    CHECK(bus.sends == 1);
    CHECK(bus.last.count == 4);
    for (std::size_t i = 0; i < 4; ++i)
    {
        CHECK(bus.last.values[i] == 0);
    }
}

void test_dshot_armed_follows_output_map()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    EscConfig c{};
    c.output_map = {{2, 0, 3, 1, 4, 5, 6, 7}};
    CHECK(esc.init(c));
    esc.write(armed_outputs(0.0f, 1.0f, 0.5f, 0.0f));
    CHECK(bus.last.values[2] == dshot::encode(dshot::kThrottleMin, false, false));  // motor 0
    CHECK(bus.last.values[0] == dshot::encode(dshot::kThrottleMax, false, false));  // motor 1
    CHECK(bus.last.values[3] == dshot::encode(1048, false, false));                 // motor 2
    CHECK(bus.last.values[1] == dshot::encode(dshot::kThrottleMin, false, false));  // motor 3
}

void test_motors_beyond_output_count_are_stopped()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    CHECK(esc.init(EscConfig{}));
    fc::ActuatorOutputs out = armed_outputs(0.5f, 0.5f, 0.5f, 0.5f);
    out.count = 2;
    esc.write(out);
    CHECK(bus.last.values[1] != 0);
    CHECK(bus.last.values[2] == 0);
    CHECK(bus.last.values[3] == 0);
}

void test_analog_pulse_widths()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    CHECK(esc.init(config_with(Protocol::kPwm)));
    esc.write(armed_outputs(0.0f, 0.5f, 1.0f, std::nanf("")));
    CHECK(bus.last.values[0] == 1000000);
    CHECK(bus.last.values[1] == 1500000);
    CHECK(bus.last.values[2] == 2000000);
    CHECK(bus.last.values[3] == 1000000);

    fc::ActuatorOutputs disarmed = armed_outputs(1.0f, 1.0f, 1.0f, 1.0f);
    disarmed.armed = false;
    CHECK(esc.init(config_with(Protocol::kOneShot125)));
    esc.write(disarmed);
    CHECK(bus.last.values[0] == 125000);
    esc.write(armed_outputs(1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(bus.last.values[0] == 250000);
}

void test_commands()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    CHECK(esc.init(config_with(Protocol::kPwm)));
    CHECK(!esc.send_command(dshot::Command::kBeep1, 0x01));
    CHECK(bus.sends == 0);

    EscConfig c{};
    c.output_map = {{3, 2, 1, 0, 4, 5, 6, 7}};
    CHECK(esc.init(c));
    CHECK(esc.send_command(dshot::Command::kBeep1, 0x01));  // logical motor 0 -> channel 3
    CHECK(bus.last.values[3] == dshot::command_frame(dshot::Command::kBeep1, false));
    CHECK(bus.last.values[0] == 0);
    CHECK(bus.last.values[1] == 0);
    CHECK(bus.last.values[2] == 0);
}

void test_erpm_is_reported_per_logical_motor()
{
    FakeEscBus bus;
    EscOutput esc(bus);
    EscConfig c = config_with(Protocol::kDShot600);
    c.bidirectional = true;
    c.output_map = {{1, 0, 2, 3, 4, 5, 6, 7}};
    CHECK(esc.init(c));

    EscOutput::Erpm erpm{};
    CHECK(!esc.read_erpm(erpm));  // board has no telemetry yet

    bus.has_erpm = true;
    bus.erpm = {{100, 200, 300, 400, 0, 0, 0, 0}};
    CHECK(esc.read_erpm(erpm));
    CHECK(erpm[0] == 200);
    CHECK(erpm[1] == 100);
    CHECK(erpm[2] == 300);
}

}  // namespace

int main()
{
    test_dshot_reference_frame();
    test_dshot_value_is_clamped();
    test_dshot_throttle_mapping();
    test_dshot_commands();
    test_config_validation();
    test_init_configures_bus();
    test_invalid_config_or_bus_refusal_sends_nothing();
    test_dshot_disarmed_sends_motor_stop();
    test_dshot_armed_follows_output_map();
    test_motors_beyond_output_count_are_stopped();
    test_analog_pulse_widths();
    test_commands();
    test_erpm_is_reported_per_logical_motor();
    return TEST_MAIN_RESULT("flight-controller esc");
}
