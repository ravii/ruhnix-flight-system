/*
 * Board support interface for the ESC. Each board under bsp/<board>/ implements it.
 */
#ifndef ESC_BSP_H
#define ESC_BSP_H

#include <stdint.h>

#include "esc/foc.h"

#ifdef __cplusplus
extern "C" {
#endif

void bsp_init(void);

/* Registers the current-loop callback, invoked from the ADC/PWM interrupt
 * at the PWM frequency with fresh phase currents and rotor angle. */
typedef void (*esc_current_loop_fn)(esc_abc_t currents, float theta_elec);
void bsp_set_current_loop(esc_current_loop_fn fn);

/* Sets phase duty cycles, each in [0, 1]. Safe to call from the ISR. */
void bsp_pwm_set(float duty_a, float duty_b, float duty_c);
void bsp_pwm_enable(int enable);

/* Latest throttle command from the flight controller, in [0, 1]. */
float bsp_throttle_command(void);

#ifdef __cplusplus
}
#endif

#endif /* ESC_BSP_H */
