/**
 * ESC entry point.
 *
 * The current loop runs in the ADC/PWM interrupt (tens of kHz) and is NOT a
 * FreeRTOS task. FreeRTOS handles the slower work: command input, speed loop,
 * protection and telemetry.
 */
#include <atomic>

#include "FreeRTOS.h"
#include "task.h"

#include "esc/bsp/board.hpp"
#include "esc/current_controller.hpp"

namespace
{

constexpr float kMaxPhaseCurrentA = 10.0f;

struct MotorControl
{
    esc::CurrentController current;
    std::atomic<float> iq_ref{0.0f};  // written by the supervisor, read in the ISR
    float dt_s = 0.0f;
};

/** Runs in ISR context: keep it short and deterministic. */
void current_loop(void* context, const esc::PhaseSample& sample)
{
    auto& mc = *static_cast<MotorControl*>(context);
    esc::CurrentController::Input in;
    in.phase_current_a = sample.current_a;
    in.theta_elec = sample.theta_elec;
    in.current_ref_a.q = mc.iq_ref.load(std::memory_order_relaxed);
    const esc::AlphaBeta v = mc.current.update(in, mc.dt_s);
    (void)v;
    // TODO: SVPWM(v, sample.bus_voltage_v) -> esc::board().set_pwm(a, b, c).
}

void supervisor_task(void* context)
{
    auto& mc = *static_cast<MotorControl*>(context);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
        // TODO: speed loop, over-current / over-temperature protection.
        mc.iq_ref.store(
            esc::board().throttle_command() * kMaxPhaseCurrentA, std::memory_order_relaxed);
    }
}

}  // namespace

int main()
{
    esc::Board& board = esc::board();
    board.init();

    static MotorControl motor_control;
    motor_control.dt_s = board.pwm_period_s();
    board.set_current_loop(&current_loop, &motor_control);

    const bool started = xTaskCreate(&supervisor_task, "supervisor", 512, &motor_control,
                             configMAX_PRIORITIES - 1, nullptr) == pdPASS;
    configASSERT(started);
    (void)started;

    vTaskStartScheduler();
    for (;;)
    {
    }
}
