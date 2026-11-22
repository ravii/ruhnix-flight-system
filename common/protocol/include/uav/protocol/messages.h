/*
 * Message definitions shared by all modules.
 *
 * Wire format: all multi-byte fields are little-endian, structs are packed.
 * Bump UAV_PROTOCOL_VERSION whenever a payload layout changes.
 */
#ifndef UAV_PROTOCOL_MESSAGES_H
#define UAV_PROTOCOL_MESSAGES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UAV_PROTOCOL_VERSION 1u

typedef enum
{
    UAV_MSG_HEARTBEAT = 0x01,
    UAV_MSG_IMU = 0x10,
    UAV_MSG_ATTITUDE = 0x11,
    UAV_MSG_MOTOR_CMD = 0x20,
    UAV_MSG_POSE_ESTIMATE = 0x30,
    UAV_MSG_SIM_SENSORS = 0x40,   /* simulator -> flight controller */
    UAV_MSG_SIM_ACTUATORS = 0x41, /* flight controller -> simulator */
} uav_msg_id_t;

typedef enum
{
    UAV_NODE_FLIGHT_CONTROLLER = 1,
    UAV_NODE_ESC = 2,
    UAV_NODE_PERCEPTION = 3,
    UAV_NODE_LOCALIZATION = 4,
} uav_node_id_t;

#define UAV_MAX_MOTORS 8u

#pragma pack(push, 1)

typedef struct
{
    uint8_t node_id;          /* uav_node_id_t */
    uint8_t protocol_version; /* UAV_PROTOCOL_VERSION */
    uint16_t status_flags;
    uint32_t uptime_ms;
} uav_msg_heartbeat_t;

typedef struct
{
    uint64_t timestamp_us;
    float gyro_rad_s[3]; /* body frame, FRD */
    float accel_m_s2[3]; /* body frame, FRD */
} uav_msg_imu_t;

typedef struct
{
    uint64_t timestamp_us;
    float quat[4]; /* w, x, y, z; body FRD -> world NED */
    float rate_rad_s[3];
} uav_msg_attitude_t;

typedef struct
{
    uint64_t timestamp_us;
    uint8_t motor_count;
    uint8_t armed;
    uint16_t throttle[UAV_MAX_MOTORS]; /* 0..2000 */
} uav_msg_motor_cmd_t;

typedef struct
{
    uint64_t timestamp_us;
    float position_m[3];   /* world NED */
    float velocity_m_s[3]; /* world NED */
    float quat[4];         /* w, x, y, z */
    float position_var[3];
} uav_msg_pose_estimate_t;

/* ---- Simulation (SITL over UDP, HITL over serial) -----------------------
 *
 * Lockstep: the simulator sends one SIM_SENSORS per physics step and waits
 * for the SIM_ACTUATORS reply (echoing time_us) before advancing.
 */

#define UAV_SIM_RC_CHANNELS 8u

enum
{
    UAV_SIM_VALID_IMU = 1u << 0,
    UAV_SIM_VALID_BARO = 1u << 1,
    UAV_SIM_VALID_MAG = 1u << 2,
    UAV_SIM_VALID_GNSS = 1u << 3,
    UAV_SIM_VALID_RC = 1u << 4,
};

typedef struct
{
    uint64_t time_us; /* simulation time; the flight controller's clock in SITL */
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
    uint8_t valid_flags;                       /* UAV_SIM_VALID_* */
    uint16_t rc_channels[UAV_SIM_RC_CHANNELS]; /* 1000..2000 us */
} uav_msg_sim_sensors_t;

typedef struct
{
    uint64_t time_us; /* echoes the uav_msg_sim_sensors_t this answers */
    uint8_t motor_count;
    uint8_t armed;
    float motors[UAV_MAX_MOTORS]; /* 0..1 */
} uav_msg_sim_actuators_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* UAV_PROTOCOL_MESSAGES_H */
