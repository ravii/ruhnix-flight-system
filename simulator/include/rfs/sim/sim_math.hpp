/** Small double-precision vector/quaternion helpers for the simulator. */
#pragma once

#include <array>
#include <cmath>

#include "rfs/sim/vehicle_model.hpp"

namespace rfs::sim
{

using Quatd = std::array<double, 4>;  ///< w, x, y, z

inline Vec3d cross(const Vec3d& a, const Vec3d& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

inline Quatd quat_multiply(const Quatd& a, const Quatd& b)
{
    return {
        a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
        a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
        a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
        a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0],
    };
}

inline Quatd quat_normalized(const Quatd& q)
{
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    return {q[0] / n, q[1] / n, q[2] / n, q[3] / n};
}

/** Rotates v by q (body -> world when q is the vehicle attitude). */
inline Vec3d quat_rotate(const Quatd& q, const Vec3d& v)
{
    // v' = v + 2 w (u x v) + 2 u x (u x v), u = vector part.
    const Vec3d u{q[1], q[2], q[3]};
    const Vec3d t = cross(u, v);
    const Vec3d t2 = cross(u, t);
    return {v[0] + 2.0 * (q[0] * t[0] + t2[0]), v[1] + 2.0 * (q[0] * t[1] + t2[1]),
        v[2] + 2.0 * (q[0] * t[2] + t2[2])};
}

/** Rotates v by the inverse of q (world -> body). */
inline Vec3d quat_rotate_inverse(const Quatd& q, const Vec3d& v)
{
    return quat_rotate({q[0], -q[1], -q[2], -q[3]}, v);
}

/** Rotation by angle |rv| about rv / |rv| (rotation vector -> quaternion). */
inline Quatd quat_from_rotation_vector(const Vec3d& rv)
{
    const double angle = std::sqrt(rv[0] * rv[0] + rv[1] * rv[1] + rv[2] * rv[2]);
    if (angle < 1e-12)
    {
        return quat_normalized({1.0, 0.5 * rv[0], 0.5 * rv[1], 0.5 * rv[2]});
    }
    const double s = std::sin(0.5 * angle) / angle;
    return {std::cos(0.5 * angle), rv[0] * s, rv[1] * s, rv[2] * s};
}

/** Z-Y-X Euler angles {roll, pitch, yaw} in rad from a body -> world quaternion. */
inline Vec3d quat_to_euler(const Quatd& q)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double roll = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
    const double sinp = std::fmax(-1.0, std::fmin(1.0, 2.0 * (w * y - z * x)));
    const double pitch = std::asin(sinp);
    const double yaw = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
    return {roll, pitch, yaw};
}

}  // namespace rfs::sim
