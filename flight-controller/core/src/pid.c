#include "fc/pid.h"

static float clampf(float v, float lim) {
    if (v > lim) {
        return lim;
    }
    if (v < -lim) {
        return -lim;
    }
    return v;
}

void fc_pid_init(fc_pid_t *pid, const fc_pid_gains_t *gains) {
    pid->gains = *gains;
    fc_pid_reset(pid);
}

void fc_pid_reset(fc_pid_t *pid) {
    pid->integrator = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->has_prev = 0;
}

float fc_pid_update(fc_pid_t *pid, float setpoint, float measurement, float dt) {
    const fc_pid_gains_t *g = &pid->gains;
    const float error = setpoint - measurement;

    pid->integrator = clampf(pid->integrator + g->ki * error * dt, g->integrator_limit);

    float derivative = 0.0f;
    if (pid->has_prev && dt > 0.0f) {
        derivative = -(measurement - pid->prev_measurement) / dt;
    }
    pid->prev_measurement = measurement;
    pid->has_prev = 1;

    return clampf(g->kp * error + pid->integrator + g->kd * derivative, g->output_limit);
}
