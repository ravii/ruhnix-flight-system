/**
 * FOC current loop as a pure step function: phase currents + rotor angle in,
 * stator voltage out. No hardware, no RTOS, no clock reads, so the same code
 * runs in the PWM interrupt on the board and against a motor model on the host.
 */
#pragma once

#include "esc/foc.hpp"

namespace esc
{

struct CurrentControllerConfig
{
    float kp = 0.5f;             ///< V/A
    float ki = 200.0f;           ///< V/(A*s)
    float voltage_limit = 6.0f;  ///< |v_d|, |v_q| clamp in volts
};

class CurrentController
{
public:
    struct Input
    {
        Abc phase_current_a;
        float theta_elec = 0.0f;  ///< rad
        Dq current_ref_a;         ///< usually d = 0, q = torque demand
    };

    explicit CurrentController(const CurrentControllerConfig& config = CurrentControllerConfig{})
      : config_(config)
    {}

    /** @return stator voltage command in the alpha/beta frame (feed to SVPWM). */
    AlphaBeta update(const Input& in, float dt_s);

    void reset() { integrator_ = Dq{}; }

    const Dq& measured_current() const { return measured_; }

private:
    CurrentControllerConfig config_;
    Dq integrator_;
    Dq measured_;
};

}  // namespace esc
