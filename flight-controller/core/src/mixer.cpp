#include "fc/mixer.hpp"

#include <algorithm>

namespace fc
{
namespace
{

/** Per-motor factors: {roll, pitch, yaw}. */
constexpr std::array<Vec3, QuadXMixer::kMotorCount> kQuadX = {{
    {-1.0f, +1.0f, +1.0f},  // 0: front-right, CCW
    {-1.0f, -1.0f, -1.0f},  // 1: rear-right,  CW
    {+1.0f, -1.0f, +1.0f},  // 2: rear-left,   CCW
    {+1.0f, +1.0f, -1.0f},  // 3: front-left,  CW
}};

}  // namespace

QuadXMixer::Output QuadXMixer::mix(const Vec3& torque, float thrust) const
{
    Output out{};
    for (std::size_t i = 0; i < kMotorCount; ++i)
    {
        out[i] = thrust + kQuadX[i][kRoll] * torque[kRoll] + kQuadX[i][kPitch] * torque[kPitch] +
                 kQuadX[i][kYaw] * torque[kYaw];
    }

    // Shift the whole set to fit in [0, 1] before clipping.
    const auto [min_it, max_it] = std::minmax_element(out.begin(), out.end());
    float shift = 0.0f;
    if (*max_it > 1.0f)
    {
        shift = 1.0f - *max_it;
    }
    else if (*min_it < 0.0f)
    {
        shift = -*min_it;
    }
    for (float& v : out)
    {
        v = std::clamp(v + shift, 0.0f, 1.0f);
    }
    return out;
}

}  // namespace fc
