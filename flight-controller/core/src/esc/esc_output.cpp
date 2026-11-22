#include "fc/esc/esc_output.hpp"

#include <cmath>

namespace fc::esc
{
namespace
{

uint32_t analog_pulse_ns(const PulseRange& range, float throttle)
{
    if (!(throttle > 0.0f))  // also catches NaN
    {
        return range.min_ns;
    }
    if (throttle >= 1.0f)
    {
        return range.max_ns;
    }
    const float span = static_cast<float>(range.max_ns - range.min_ns);
    return range.min_ns + static_cast<uint32_t>(std::lround(throttle * span));
}

}  // namespace

bool EscOutput::init(const EscConfig& config)
{
    initialized_ = false;
    if (!is_valid(config) ||
        !bus_.configure(config.protocol, config.bidirectional, config.motor_count))
    {
        return false;
    }
    config_ = config;
    initialized_ = true;
    return true;
}

EscFrame EscOutput::encode(const ActuatorOutputs& outputs) const
{
    EscFrame frame;
    if (!initialized_)
    {
        return frame;
    }
    frame.count = config_.motor_count;

    const bool dshot = is_dshot(config_.protocol);
    const bool bidir = config_.bidirectional;
    const PulseRange pulse = analog_pulse_range(config_.protocol);
    for (std::size_t i = 0; i < config_.motor_count; ++i)
    {
        const bool run = outputs.armed && i < outputs.count;
        uint32_t value = 0;
        if (dshot)
        {
            value = run ? dshot::encode(dshot::throttle_value(outputs.motors[i]), false, bidir) :
                          dshot::command_frame(dshot::Command::kMotorStop, bidir);
        }
        else
        {
            value = run ? analog_pulse_ns(pulse, outputs.motors[i]) : pulse.min_ns;
        }
        frame.values[config_.output_map[i]] = value;
    }
    return frame;
}

void EscOutput::write(const ActuatorOutputs& outputs)
{
    if (initialized_)
    {
        bus_.send(encode(outputs));
    }
}

bool EscOutput::send_command(dshot::Command command, uint8_t motor_mask)
{
    if (!initialized_ || !is_dshot(config_.protocol))
    {
        return false;
    }
    EscFrame frame;
    frame.count = config_.motor_count;
    const bool bidir = config_.bidirectional;
    for (std::size_t i = 0; i < config_.motor_count; ++i)
    {
        const bool selected = ((static_cast<unsigned>(motor_mask) >> i) & 1u) != 0;
        frame.values[config_.output_map[i]] =
            dshot::command_frame(selected ? command : dshot::Command::kMotorStop, bidir);
    }
    bus_.send(frame);
    return true;
}

bool EscOutput::read_erpm(Erpm& out)
{
    Erpm by_channel{};
    if (!initialized_ || !bus_.read_erpm(by_channel))
    {
        return false;
    }
    out = Erpm{};
    for (std::size_t i = 0; i < config_.motor_count; ++i)
    {
        out[i] = by_channel[config_.output_map[i]];
    }
    return true;
}

}  // namespace fc::esc
