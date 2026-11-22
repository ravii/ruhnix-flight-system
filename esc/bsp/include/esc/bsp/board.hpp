/**
 * Hardware abstraction for the ESC. Each directory under bsp/ implements it:
 * real power stages, or (later) a motor model for host simulation.
 */
#pragma once

#include <cstdint>

#include "esc/foc.hpp"

namespace esc
{

/** One ADC sample set, taken synchronously with the PWM period. */
struct PhaseSample
{
    Abc current_a;
    float theta_elec = 0.0f;  ///< from encoder / observer, rad
    float bus_voltage_v = 0.0f;
};

class Board
{
public:
    /**
     * Current-loop callback. Called from the ADC/PWM interrupt at the PWM rate
     * (hardware) or once per motor-model step (simulation). Must not call the RTOS.
     */
    using CurrentLoopFn = void (*)(void* context, const PhaseSample& sample);

    virtual void init() = 0;
    virtual uint64_t time_us() = 0;
    virtual float pwm_period_s() const = 0;

    virtual void set_current_loop(CurrentLoopFn fn, void* context) = 0;

    /** Phase duty cycles, each in [0, 1]. Safe to call from the current loop. */
    virtual void set_pwm(float duty_a, float duty_b, float duty_c) = 0;
    virtual void enable_pwm(bool enable) = 0;

    /** Latest throttle command from the flight controller, in [0, 1]. */
    virtual float throttle_command() = 0;

protected:
    ~Board() = default;
};

/** Defined by the BSP selected with -DESC_BOARD. */
Board& board();

}  // namespace esc
