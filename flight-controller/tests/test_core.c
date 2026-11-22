#include <math.h>
#include <stdio.h>

#include "fc/mixer.h"
#include "fc/pid.h"

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                            \
        }                                                                          \
    } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-5f)

static void test_pid_proportional(void) {
    fc_pid_gains_t g = {2.0f, 0.0f, 0.0f, 1.0f, 10.0f};
    fc_pid_t pid;
    fc_pid_init(&pid, &g);
    CHECK(NEAR(fc_pid_update(&pid, 1.0f, 0.25f, 0.001f), 1.5f));
}

static void test_pid_limits(void) {
    fc_pid_gains_t g = {0.0f, 100.0f, 0.0f, 0.5f, 0.3f};
    fc_pid_t pid;
    fc_pid_init(&pid, &g);
    for (int i = 0; i < 1000; ++i) {
        fc_pid_update(&pid, 1.0f, 0.0f, 0.01f);
    }
    CHECK(NEAR(pid.integrator, 0.5f));                            /* integrator clamp */
    CHECK(NEAR(fc_pid_update(&pid, 1.0f, 0.0f, 0.01f), 0.3f));   /* output clamp */
}

static void test_mixer_hover(void) {
    float m[FC_MIXER_QUAD_X_MOTORS];
    fc_mixer_quad_x(0.0f, 0.0f, 0.0f, 0.5f, m);
    for (int i = 0; i < FC_MIXER_QUAD_X_MOTORS; ++i) {
        CHECK(NEAR(m[i], 0.5f));
    }
}

static void test_mixer_roll_right(void) {
    float m[FC_MIXER_QUAD_X_MOTORS];
    fc_mixer_quad_x(0.1f, 0.0f, 0.0f, 0.5f, m);
    /* Rolling right: left motors (2, 3) speed up, right motors (0, 1) slow down. */
    CHECK(m[2] > m[0] && m[2] > m[1]);
    CHECK(m[3] > m[0] && m[3] > m[1]);
}

static void test_mixer_saturation_keeps_attitude(void) {
    float m[FC_MIXER_QUAD_X_MOTORS];
    fc_mixer_quad_x(0.2f, 0.0f, 0.0f, 1.0f, m);
    /* Differential must survive even at full throttle. */
    CHECK(NEAR(m[3] - m[0], 0.4f));
    for (int i = 0; i < FC_MIXER_QUAD_X_MOTORS; ++i) {
        CHECK(m[i] >= 0.0f && m[i] <= 1.0f);
    }
}

int main(void) {
    test_pid_proportional();
    test_pid_limits();
    test_mixer_hover();
    test_mixer_roll_right();
    test_mixer_saturation_keeps_attitude();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all flight-controller core tests passed\n");
    return 0;
}
