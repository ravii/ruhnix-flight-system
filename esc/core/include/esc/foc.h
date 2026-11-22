/* Field-oriented control transforms. Platform-independent; no RTOS/hardware deps. */
#ifndef ESC_FOC_H
#define ESC_FOC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float a, b, c;
} esc_abc_t;

typedef struct {
    float alpha, beta;
} esc_alphabeta_t;

typedef struct {
    float d, q;
} esc_dq_t;

/* Amplitude-invariant Clarke transform (assumes a + b + c = 0). */
esc_alphabeta_t esc_clarke(esc_abc_t in);

/* Park transform; theta is the electrical angle in radians. */
esc_dq_t esc_park(esc_alphabeta_t in, float theta);

esc_alphabeta_t esc_inv_park(esc_dq_t in, float theta);

#ifdef __cplusplus
}
#endif

#endif /* ESC_FOC_H */
