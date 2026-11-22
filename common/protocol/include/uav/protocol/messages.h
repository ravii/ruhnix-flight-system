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

typedef enum {
    UAV_MSG_HEARTBEAT = 0x01,
    UAV_MSG_IMU = 0x10,
    UAV_MSG_ATTITUDE = 0x11,
    UAV_MSG_MOTOR_CMD = 0x20,
    UAV_MSG_POSE_ESTIMATE = 0x30,
} uav_msg_id_t;

typedef enum {
    UAV_NODE_FLIGHT_CONTROLLER = 1,
    UAV_NODE_ESC = 2,
    UAV_NODE_PERCEPTION = 3,
    UAV_NODE_LOCALIZATION = 4,
} uav_node_id_t;

#define UAV_MAX_MOTORS 8u

#pragma pack(push, 1)

typedef struct {
    uint8_t node_id;          /* uav_node_id_t */
    uint8_t protocol_version; /* UAV_PROTOCOL_VERSION */
    uint16_t status_flags;
    uint32_t uptime_ms;
} uav_msg_heartbeat_t;

typedef struct {
    uint64_t timestamp_us;
    float gyro_rad_s[3]; /* body frame, FRD */
    float accel_m_s2[3]; /* body frame, FRD */
} uav_msg_imu_t;

typedef struct {
    uint64_t timestamp_us;
    float quat[4]; /* w, x, y, z; body FRD -> world NED */
    float rate_rad_s[3];
} uav_msg_attitude_t;

typedef struct {
    uint64_t timestamp_us;
    uint8_t motor_count;
    uint8_t armed;
    uint16_t throttle[UAV_MAX_MOTORS]; /* 0..2000 */
} uav_msg_motor_cmd_t;

typedef struct {
    uint64_t timestamp_us;
    float position_m[3];   /* world NED */
    float velocity_m_s[3]; /* world NED */
    float quat[4];         /* w, x, y, z */
    float position_var[3];
} uav_msg_pose_estimate_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* UAV_PROTOCOL_MESSAGES_H */
