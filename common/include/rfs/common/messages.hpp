/**
 * Message definitions shared by all modules.
 *
 * Wire format: all multi-byte fields are little-endian, structs are packed.
 * Bump kProtocolVersion whenever a payload layout changes; the size
 * static_asserts at the end of this file pin every layout.
 *
 * Packed structs: copy fields element-wise. Never bind references or pointers
 * to their members (unaligned access, -Waddress-of-packed-member); that is
 * also why arrays here are C arrays, not std::array.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace rfs::common
{

inline constexpr uint8_t kProtocolVersion = 1;

enum class MsgId : uint8_t
{
    kHeartbeat = 0x01,
    kImu = 0x10,
    kAttitude = 0x11,
    kMotorCmd = 0x20,
    kPoseEstimate = 0x30,
    kSimSensors = 0x40,    ///< simulator -> flight controller
    kSimActuators = 0x41,  ///< flight controller -> simulator
};

enum class NodeId : uint8_t
{
    kFlightController = 1,
    kEsc = 2,
    kPerception = 3,
    kLocalization = 4,
};

inline constexpr std::size_t kMaxMotors = 8;

#pragma pack(push, 1)

struct HeartbeatMsg
{
    NodeId node_id;
    uint8_t protocol_version;  ///< kProtocolVersion
    uint16_t status_flags;
    uint32_t uptime_ms;
};

struct ImuMsg
{
    uint64_t timestamp_us;
    float gyro_rad_s[3];  ///< body frame, FRD
    float accel_m_s2[3];  ///< body frame, FRD
};

struct AttitudeMsg
{
    uint64_t timestamp_us;
    float quat[4];  ///< w, x, y, z; body FRD -> world NED
    float rate_rad_s[3];
};

struct MotorCmdMsg
{
    uint64_t timestamp_us;
    uint8_t motor_count;
    uint8_t armed;
    uint16_t throttle[kMaxMotors];  ///< 0..2000
};

struct PoseEstimateMsg
{
    uint64_t timestamp_us;
    float position_m[3];    ///< world NED
    float velocity_m_s[3];  ///< world NED
    float quat[4];          ///< w, x, y, z
    float position_var[3];
};

// ---- Simulation (SITL over UDP, HITL over serial) ------------------------
//
// Lockstep: the simulator sends one SimSensorsMsg per physics step and waits
// for the SimActuatorsMsg reply (echoing time_us) before advancing.

inline constexpr std::size_t kSimRcChannels = 8;

/** SimSensorsMsg::valid_flags bits. */
inline constexpr uint8_t kSimValidImu = 1u << 0;
inline constexpr uint8_t kSimValidBaro = 1u << 1;
inline constexpr uint8_t kSimValidMag = 1u << 2;
inline constexpr uint8_t kSimValidGnss = 1u << 3;
inline constexpr uint8_t kSimValidRc = 1u << 4;

struct SimSensorsMsg
{
    uint64_t time_us;  ///< simulation time; the flight controller's clock in SITL
    float gyro_rad_s[3];
    float accel_m_s2[3];
    float mag_gauss[3];
    float baro_pressure_pa;
    float baro_temperature_c;
    int32_t gnss_lat_e7;
    int32_t gnss_lon_e7;
    float gnss_alt_m;
    float gnss_vel_ned_m_s[3];
    uint8_t gnss_fix_type;
    uint8_t valid_flags;                   ///< kSimValid* bits
    uint16_t rc_channels[kSimRcChannels];  ///< 1000..2000 us
};

struct SimActuatorsMsg
{
    uint64_t time_us;  ///< echoes the SimSensorsMsg this answers
    uint8_t motor_count;
    uint8_t armed;
    float motors[kMaxMotors];  ///< 0..1
};

#pragma pack(pop)

// Wire layouts. A failing size check means the payload layout changed:
// update the expected size and bump kProtocolVersion.
static_assert(sizeof(HeartbeatMsg) == 8, "HeartbeatMsg layout changed");
static_assert(sizeof(ImuMsg) == 32, "ImuMsg layout changed");
static_assert(sizeof(AttitudeMsg) == 36, "AttitudeMsg layout changed");
static_assert(sizeof(MotorCmdMsg) == 26, "MotorCmdMsg layout changed");
static_assert(sizeof(PoseEstimateMsg) == 60, "PoseEstimateMsg layout changed");
static_assert(sizeof(SimSensorsMsg) == 94, "SimSensorsMsg layout changed");
static_assert(sizeof(SimActuatorsMsg) == 42, "SimActuatorsMsg layout changed");

static_assert(
    std::is_trivially_copyable_v<SimSensorsMsg> && std::is_trivially_copyable_v<SimActuatorsMsg>,
    "messages are sent with memcpy");

}  // namespace rfs::common
