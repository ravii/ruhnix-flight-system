/**
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
#pragma once

#include <array>
#include <cstddef>

#include "rfs/fc/types.hpp"

namespace rfs::fc
{

class QuadXMixer
{
public:
    static constexpr std::size_t kMotorCount = 4;
    using Output = std::array<float, kMotorCount>;

    /**
     * @param torque  normalized roll/pitch/yaw demand, each in [-1, 1]
     * @param thrust  collective thrust in [0, 1]
     * @return motor outputs in [0, 1]. When saturated, attitude authority is
     *         preserved at the expense of collective thrust.
     */
    Output mix(const Vec3& torque, float thrust) const;
};

}  // namespace rfs::fc
