#include "fc/mixer.h"

/* Per-motor factors: {roll, pitch, yaw}. */
static const float kQuadX[FC_MIXER_QUAD_X_MOTORS][3] = {
    {-1.0f, +1.0f, +1.0f}, /* 0: front-right, CCW */
    {-1.0f, -1.0f, -1.0f}, /* 1: rear-right,  CW  */
    {+1.0f, -1.0f, +1.0f}, /* 2: rear-left,   CCW */
    {+1.0f, +1.0f, -1.0f}, /* 3: front-left,  CW  */
};

void fc_mixer_quad_x(float roll, float pitch, float yaw, float throttle,
                     float out[FC_MIXER_QUAD_X_MOTORS]) {
    float max = -1e9f;
    float min = 1e9f;
    for (int i = 0; i < FC_MIXER_QUAD_X_MOTORS; ++i) {
        out[i] = throttle + kQuadX[i][0] * roll + kQuadX[i][1] * pitch + kQuadX[i][2] * yaw;
        if (out[i] > max) {
            max = out[i];
        }
        if (out[i] < min) {
            min = out[i];
        }
    }

    /* Shift the whole set to fit in [0, 1] before clipping. */
    float shift = 0.0f;
    if (max > 1.0f) {
        shift = 1.0f - max;
    } else if (min < 0.0f) {
        shift = -min;
    }
    for (int i = 0; i < FC_MIXER_QUAD_X_MOTORS; ++i) {
        float v = out[i] + shift;
        out[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
}
