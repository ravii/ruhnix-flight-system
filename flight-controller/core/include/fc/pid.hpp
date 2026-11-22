/** Platform-independent PID controller. No RTOS or hardware dependencies. */
#pragma once

namespace fc
{

struct PidGains
{
    float kp = 0.0f;
    float ki = 0.0f;
    float kd = 0.0f;
    float integrator_limit = 0.0f;  ///< |integrator term| is clamped to this value
    float output_limit = 0.0f;      ///< |output| is clamped to this value
};

class Pid
{
public:
    explicit Pid(const PidGains& gains = PidGains{}) : gains_(gains) {}

    void set_gains(const PidGains& gains) { gains_ = gains; }
    const PidGains& gains() const { return gains_; }

    void reset();

    /** Derivative acts on the measurement to avoid setpoint kick. dt in seconds. */
    float update(float setpoint, float measurement, float dt);

    float integrator() const { return integrator_; }

private:
    PidGains gains_;
    float integrator_ = 0.0f;
    float prev_measurement_ = 0.0f;
    bool has_prev_ = false;
};

}  // namespace fc
