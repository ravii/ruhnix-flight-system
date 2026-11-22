/*
 * ESC entry point.
 *
 * The current loop runs in the ADC/PWM interrupt (tens of kHz) and is NOT a
 * FreeRTOS task. FreeRTOS handles the slower work: command input, speed loop,
 * protection and telemetry.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "esc/foc.h"
#include "esc_bsp.h"

#define SUPERVISOR_TASK_PRIO (configMAX_PRIORITIES - 1)
#define SUPERVISOR_PERIOD_MS 1u

static volatile esc_dq_t g_current_ref = {0.0f, 0.0f};

/* Runs in ISR context: keep it short and deterministic. */
static void current_loop(esc_abc_t currents, float theta_elec) {
    esc_dq_t i_dq = esc_park(esc_clarke(currents), theta_elec);
    (void)i_dq;
    /* TODO: d/q current PI controllers -> v_dq -> inverse Park -> SVPWM -> bsp_pwm_set(). */
}

static void supervisor_task(void *arg) {
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
        /* TODO: speed loop, over-current / over-temperature protection. */
        const float throttle = bsp_throttle_command();
        g_current_ref.q = throttle; /* placeholder mapping */
    }
}

int main(void) {
    bsp_init();
    bsp_set_current_loop(current_loop);

    xTaskCreate(supervisor_task, "supervisor", 384, NULL, SUPERVISOR_TASK_PRIO, NULL);

    vTaskStartScheduler();
    for (;;) {
    }
}

void esc_assert_failed(const char *file, int line) {
    (void)file;
    (void)line;
    bsp_pwm_enable(0);
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void) {
    esc_assert_failed(__FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    esc_assert_failed(__FILE__, __LINE__);
}
