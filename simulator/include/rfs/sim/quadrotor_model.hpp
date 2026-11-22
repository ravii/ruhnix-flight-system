/**
 * Built-in quad-X rigid-body model.
 *
 * Motor layout and spin directions match rfs::fc::QuadXMixer (body FRD, seen
 * from above):
 *
 *        front
 *     3(CW)  0(CCW)
 *         \  /
 *          ><
 *         /  \
 *     2(CCW) 1(CW)
 *        rear
 *
 * Per motor: the normalized command u in [0, 1] drives the normalized rotor
 * speed s through a first-order lag (motor_time_constant_s); thrust is
 * max_thrust_n * s^2 along body -z, and the reaction torque is
 * yaw_moment_per_thrust_m * thrust about body z (+z for CCW rotors, which
 * push the body clockwise seen from above). Disarmed = all commands 0.
 *
 * Also modelled: linear translational drag, linear rotational damping,
 * gyroscopic coupling (omega x I omega), and a flat ground at NED z = 0 that
 * holds the vehicle at rest until thrust exceeds weight. Not modelled: ground
 * tipping and friction, blade flapping, battery sag, wind.
 */
#pragma once

#include <array>

#include "rfs/sim/vehicle_model.hpp"

namespace rfs::sim
{

struct QuadrotorParams
{
    double mass_kg = 1.0;
    Vec3d inertia_kg_m2{0.005, 0.005, 0.009};   ///< diagonal, body FRD
    double arm_length_m = 0.125;                ///< center to motor (250 mm diagonal)
    double max_thrust_n = 7.5;                  ///< per motor at u = 1
    double yaw_moment_per_thrust_m = 0.015;     ///< reaction torque / thrust
    double motor_time_constant_s = 0.02;        ///< rotor speed lag
    double linear_drag_n_per_m_s = 0.25;        ///< F = -k v (world frame)
    double angular_damping_nm_per_rad_s = 0.0;  ///< tau = -k omega (body frame)
    double gravity_m_s2 = 9.80665;
};

class QuadrotorModel final : public VehicleModel
{
public:
    static constexpr std::size_t kMotorCount = 4;

    explicit QuadrotorModel(const QuadrotorParams& params = QuadrotorParams{});

    /** Resets the rigid body; rotors start stopped. */
    void reset(const VehicleState& initial) override;
    void step(const common::SimActuatorsMsg& actuators, double dt_s) override;
    const VehicleState& state() const override { return state_; }

    const QuadrotorParams& params() const { return params_; }
    /** Normalized rotor speeds s in [0, 1]. */
    const std::array<double, kMotorCount>& rotor_speeds() const { return rotor_speed_; }
    /** True while the vehicle rests on the ground. */
    bool on_ground() const { return on_ground_; }

    /** Motor command that balances gravity when all four motors are equal. */
    double hover_command() const;

private:
    QuadrotorParams params_;
    VehicleState state_;
    std::array<double, kMotorCount> rotor_speed_{};
    bool on_ground_ = false;
};

}  // namespace rfs::sim
