/* Platform-independent PID controller. No RTOS or hardware dependencies. */
#ifndef FC_PID_H
#define FC_PID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;
    float ki;
    float kd;
    float integrator_limit; /* |integrator term| is clamped to this value */
    float output_limit;     /* |output| is clamped to this value */
} fc_pid_gains_t;

typedef struct {
    fc_pid_gains_t gains;
    float integrator;
    float prev_measurement;
    int has_prev;
} fc_pid_t;

void fc_pid_init(fc_pid_t *pid, const fc_pid_gains_t *gains);
void fc_pid_reset(fc_pid_t *pid);

/* Derivative acts on the measurement to avoid setpoint kick. dt in seconds. */
float fc_pid_update(fc_pid_t *pid, float setpoint, float measurement, float dt);

#ifdef __cplusplus
}
#endif

#endif /* FC_PID_H */
