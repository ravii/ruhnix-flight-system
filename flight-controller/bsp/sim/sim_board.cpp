#include "sim_board.hpp"

namespace fc::sim
{
// Protocol structs are packed: copy element-wise. Never bind references or
// pointers to their members (unaligned access, -Waddress-of-packed-member).
#define FC_SIM_VEC3(arr)             \
    Vec3                             \
    {                                \
        (arr)[0], (arr)[1], (arr)[2] \
    }

bool SimBoard::take(uint8_t flag)
{
    if ((unread_ & flag) == 0)
    {
        return false;
    }
    unread_ = static_cast<uint8_t>(unread_ & ~flag);
    return true;
}

bool SimBoard::wait_imu(ImuSample& out, uint32_t timeout_ms)
{
    if (!backend_.receive_sensors(packet_, timeout_ms))
    {
        return false;
    }
    unread_ = packet_.valid_flags;
    if (!take(UAV_SIM_VALID_IMU))
    {
        return false;
    }
    out.timestamp_us = packet_.time_us;
    out.gyro_rad_s = FC_SIM_VEC3(packet_.gyro_rad_s);
    out.accel_m_s2 = FC_SIM_VEC3(packet_.accel_m_s2);
    return true;
}

bool SimBoard::read_baro(BaroSample& out)
{
    if (!take(UAV_SIM_VALID_BARO))
    {
        return false;
    }
    out.timestamp_us = packet_.time_us;
    out.pressure_pa = packet_.baro_pressure_pa;
    out.temperature_c = packet_.baro_temperature_c;
    return true;
}

bool SimBoard::read_mag(MagSample& out)
{
    if (!take(UAV_SIM_VALID_MAG))
    {
        return false;
    }
    out.timestamp_us = packet_.time_us;
    out.field_gauss = FC_SIM_VEC3(packet_.mag_gauss);
    return true;
}

bool SimBoard::read_gnss(GnssSample& out)
{
    if (!take(UAV_SIM_VALID_GNSS))
    {
        return false;
    }
    out.timestamp_us = packet_.time_us;
    out.lat_e7 = packet_.gnss_lat_e7;
    out.lon_e7 = packet_.gnss_lon_e7;
    out.alt_m = packet_.gnss_alt_m;
    out.vel_ned_m_s = FC_SIM_VEC3(packet_.gnss_vel_ned_m_s);
    out.fix_type = packet_.gnss_fix_type;
    return true;
}

void SimBoard::read_rc(RcInput& out)
{
    out.timestamp_us = packet_.time_us;
    out.valid = (packet_.valid_flags & UAV_SIM_VALID_RC) != 0;
    static_assert(RcInput::kChannels == UAV_SIM_RC_CHANNELS, "RC channel count mismatch");
    for (std::size_t i = 0; i < RcInput::kChannels; ++i)
    {
        out.channels[i] = packet_.rc_channels[i];
    }
}

void SimBoard::write_motors(const ActuatorOutputs& outputs)
{
    static_assert(ActuatorOutputs::kMaxMotors == UAV_MAX_MOTORS, "motor count mismatch");
    uav_msg_sim_actuators_t msg{};
    msg.time_us = packet_.time_us;
    msg.motor_count = outputs.count;
    msg.armed = outputs.armed ? 1 : 0;
    for (std::size_t i = 0; i < ActuatorOutputs::kMaxMotors; ++i)
    {
        msg.motors[i] = outputs.motors[i];
    }
    backend_.send_actuators(msg);
}

}  // namespace fc::sim
