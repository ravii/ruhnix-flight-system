#include "rfs/sim/quadrotor_model.hpp"

#include <algorithm>
#include <cmath>

#include "rfs/sim/sim_math.hpp"

namespace rfs::sim
{
namespace
{

struct MotorGeometry
{
    double x;     ///< forward, in units of arm_length / sqrt(2)
    double y;     ///< right
    double spin;  ///< +1 CCW (reaction torque +z), -1 CW
};

/** Same order as rfs::fc::QuadXMixer. */
constexpr std::array<MotorGeometry, QuadrotorModel::kMotorCount> kMotors = {{
    {+1.0, +1.0, +1.0},  // 0: front-right, CCW
    {-1.0, +1.0, -1.0},  // 1: rear-right,  CW
    {-1.0, -1.0, +1.0},  // 2: rear-left,   CCW
    {+1.0, -1.0, -1.0},  // 3: front-left,  CW
}};

}  // namespace

QuadrotorModel::QuadrotorModel(const QuadrotorParams& params) : params_(params)
{
    reset(VehicleState{});
}

void QuadrotorModel::reset(const VehicleState& initial)
{
    state_ = initial;
    state_.attitude_wxyz = quat_normalized(state_.attitude_wxyz);
    rotor_speed_.fill(0.0);

    // Consistent accelerometer reading before the first step: at rest or in free fall.
    const bool resting = state_.position_ned_m[2] >= 0.0 && state_.velocity_ned_m_s[2] >= 0.0;
    on_ground_ = resting;
    state_.specific_force_body_m_s2 =
        resting ? quat_rotate_inverse(state_.attitude_wxyz, {0.0, 0.0, -params_.gravity_m_s2}) :
                  Vec3d{};
}

double QuadrotorModel::hover_command() const
{
    const double per_motor = params_.mass_kg * params_.gravity_m_s2 /
                             (static_cast<double>(kMotorCount) * params_.max_thrust_n);
    return std::sqrt(per_motor);
}

void QuadrotorModel::step(const common::SimActuatorsMsg& actuators, double dt_s)
{
    if (dt_s <= 0.0)
    {
        return;
    }
    const QuadrotorParams& p = params_;

    // ---- motors: first-order lag, exact for a command held over dt ---------
    const double alpha = 1.0 - std::exp(-dt_s / p.motor_time_constant_s);
    double thrust_total = 0.0;
    Vec3d torque{};
    const double arm = p.arm_length_m / std::sqrt(2.0);
    for (std::size_t i = 0; i < kMotorCount; ++i)
    {
        double u = 0.0;
        if (actuators.armed != 0 && i < actuators.motor_count)
        {
            u = std::clamp(static_cast<double>(actuators.motors[i]), 0.0, 1.0);
        }
        rotor_speed_[i] += (u - rotor_speed_[i]) * alpha;

        const double thrust = p.max_thrust_n * rotor_speed_[i] * rotor_speed_[i];
        thrust_total += thrust;
        // r x F with r = (x, y, 0), F = (0, 0, -thrust).
        torque[0] += -kMotors[i].y * arm * thrust;
        torque[1] += kMotors[i].x * arm * thrust;
        torque[2] += kMotors[i].spin * p.yaw_moment_per_thrust_m * thrust;
    }

    VehicleState& s = state_;
    const Vec3d thrust_body{0.0, 0.0, -thrust_total};
    const Vec3d thrust_world = quat_rotate(s.attitude_wxyz, thrust_body);
    Vec3d accel_world{};
    for (std::size_t k = 0; k < 3; ++k)
    {
        accel_world[k] =
            (thrust_world[k] - p.linear_drag_n_per_m_s * s.velocity_ned_m_s[k]) / p.mass_kg;
    }
    accel_world[2] += p.gravity_m_s2;

    // ---- ground: rest until the net force lifts the vehicle -----------------
    if (s.position_ned_m[2] >= 0.0 && s.velocity_ned_m_s[2] >= 0.0 && accel_world[2] >= 0.0)
    {
        on_ground_ = true;
        s.position_ned_m[2] = 0.0;
        s.velocity_ned_m_s = Vec3d{};
        s.angular_velocity_body_rad_s = Vec3d{};
        s.specific_force_body_m_s2 =
            quat_rotate_inverse(s.attitude_wxyz, {0.0, 0.0, -p.gravity_m_s2});
        return;
    }
    on_ground_ = false;

    // ---- rotation: I w' = tau - w x (I w) - k w ------------------------------
    Vec3d& w = s.angular_velocity_body_rad_s;
    const Vec3d iw{p.inertia_kg_m2[0] * w[0], p.inertia_kg_m2[1] * w[1], p.inertia_kg_m2[2] * w[2]};
    const Vec3d gyroscopic = cross(w, iw);
    for (std::size_t k = 0; k < 3; ++k)
    {
        const double net = torque[k] - gyroscopic[k] - p.angular_damping_nm_per_rad_s * w[k];
        w[k] += net / p.inertia_kg_m2[k] * dt_s;
    }
    s.attitude_wxyz = quat_normalized(quat_multiply(
        s.attitude_wxyz, quat_from_rotation_vector({w[0] * dt_s, w[1] * dt_s, w[2] * dt_s})));

    // ---- translation (semi-implicit Euler) -----------------------------------
    for (std::size_t k = 0; k < 3; ++k)
    {
        s.velocity_ned_m_s[k] += accel_world[k] * dt_s;
        s.position_ned_m[k] += s.velocity_ned_m_s[k] * dt_s;
    }
    // Ideal accelerometer: everything except gravity, in body axes.
    s.specific_force_body_m_s2 = quat_rotate_inverse(
        s.attitude_wxyz, {accel_world[0], accel_world[1], accel_world[2] - p.gravity_m_s2});

    // Touchdown: inelastic, the next step decides whether it stays down.
    if (s.position_ned_m[2] > 0.0)
    {
        s.position_ned_m[2] = 0.0;
        s.velocity_ned_m_s[2] = 0.0;
    }
}

}  // namespace rfs::sim
