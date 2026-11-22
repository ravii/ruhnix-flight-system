/** Vehicle dynamics interface: actuator commands in, rigid-body state out. */
#pragma once

#include <array>

#include "rfs/common/messages.hpp"

namespace rfs::sim
{

using Vec3d = std::array<double, 3>;

/** Ground-truth state. World frame NED, body frame FRD, SI units. */
struct VehicleState
{
    Vec3d position_ned_m{};
    Vec3d velocity_ned_m_s{};
    std::array<double, 4> attitude_wxyz{1.0, 0.0, 0.0, 0.0};  ///< body -> world
    Vec3d angular_velocity_body_rad_s{};
    Vec3d specific_force_body_m_s2{};  ///< what an ideal accelerometer measures
};

/**
 * Implementations (to be added): a built-in quadrotor model (rigid body +
 * first-order motors + drag), or a bridge to an external engine (Gazebo ...).
 */
class VehicleModel
{
public:
    virtual ~VehicleModel() = default;

    virtual void reset(const VehicleState& initial) = 0;

    /** Advances the physics by dt_s holding the given actuator command. */
    virtual void step(const common::SimActuatorsMsg& actuators, double dt_s) = 0;

    virtual const VehicleState& state() const = 0;
};

}  // namespace rfs::sim
