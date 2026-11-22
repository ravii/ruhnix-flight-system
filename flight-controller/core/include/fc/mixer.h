/*
 * Quad-X motor mixer. Body frame FRD; viewed from above:
 *
 *        front
 *     3(CW)  0(CCW)
 *         \  /
 *          ><
 *         /  \
 *     2(CCW) 1(CW)
 *        rear
 *
 * Positive roll = right side down, positive pitch = nose up,
 * positive yaw = nose right (clockwise seen from above).
 */
#ifndef FC_MIXER_H
#define FC_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

#define FC_MIXER_QUAD_X_MOTORS 4

/* roll/pitch/yaw in [-1, 1], throttle in [0, 1]; outputs in [0, 1].
 * When saturated, attitude authority is preserved at the expense of throttle. */
void fc_mixer_quad_x(float roll, float pitch, float yaw, float throttle,
                     float out[FC_MIXER_QUAD_X_MOTORS]);

#ifdef __cplusplus
}
#endif

#endif /* FC_MIXER_H */
