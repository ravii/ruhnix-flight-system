#include <math.h>
#include <stdio.h>

#include "esc/foc.h"

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                            \
        }                                                                          \
    } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-4f)

static const float kTwoPiOver3 = 2.09439510239319549f;

/* A balanced three-phase current aligned with theta maps to pure d-axis. */
static void test_balanced_currents_map_to_d_axis(void) {
    for (int k = 0; k < 16; ++k) {
        const float theta = (float)k * 0.4f;
        esc_abc_t abc = {cosf(theta), cosf(theta - kTwoPiOver3), cosf(theta + kTwoPiOver3)};
        esc_dq_t dq = esc_park(esc_clarke(abc), theta);
        CHECK(NEAR(dq.d, 1.0f));
        CHECK(NEAR(dq.q, 0.0f));
    }
}

static void test_park_roundtrip(void) {
    esc_alphabeta_t ab = {0.3f, -0.7f};
    esc_alphabeta_t back = esc_inv_park(esc_park(ab, 1.234f), 1.234f);
    CHECK(NEAR(back.alpha, ab.alpha));
    CHECK(NEAR(back.beta, ab.beta));
}

int main(void) {
    test_balanced_currents_map_to_d_axis();
    test_park_roundtrip();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all esc core tests passed\n");
    return 0;
}
