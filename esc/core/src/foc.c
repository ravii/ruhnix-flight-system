#include "esc/foc.h"

#include <math.h>

#define ESC_INV_SQRT3 0.57735026918962576f

esc_alphabeta_t esc_clarke(esc_abc_t in) {
    esc_alphabeta_t out;
    out.alpha = in.a;
    out.beta = (in.a + 2.0f * in.b) * ESC_INV_SQRT3;
    return out;
}

esc_dq_t esc_park(esc_alphabeta_t in, float theta) {
    const float s = sinf(theta);
    const float c = cosf(theta);
    esc_dq_t out;
    out.d = in.alpha * c + in.beta * s;
    out.q = -in.alpha * s + in.beta * c;
    return out;
}

esc_alphabeta_t esc_inv_park(esc_dq_t in, float theta) {
    const float s = sinf(theta);
    const float c = cosf(theta);
    esc_alphabeta_t out;
    out.alpha = in.d * c - in.q * s;
    out.beta = in.d * s + in.q * c;
    return out;
}
