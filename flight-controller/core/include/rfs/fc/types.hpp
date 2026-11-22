/**
 * Data types shared by the flight-controller core, the board interface and
 * the simulator bridge. Frames: body FRD, world NED. Units: SI.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rfs::fc
{

using Vec3 = std::array<float, 3>;

enum Axis : std::size_t
{
    kRoll = 0,
    kPitch = 1,
    kYaw = 2,
};

struct ImuSample
{
    uint64_t timestamp_us = 0;
    Vec3 gyro_rad_s{};
    Vec3 accel_m_s2{};
};

struct BaroSample
{
    uint64_t timestamp_us = 0;
    float pressure_pa = 0.0f;
    float temperature_c = 0.0f;
};

struct MagSample
{
    uint64_t timestamp_us = 0;
    Vec3 field_gauss{};
};

struct GnssSample
{
    uint64_t timestamp_us = 0;
    int32_t lat_e7 = 0;
    int32_t lon_e7 = 0;
    float alt_m = 0.0f;
    Vec3 vel_ned_m_s{};
    uint8_t fix_type = 0;
};

/** RC channels in microseconds (1000..2000). Channel map: see RcChannel. */
struct RcInput
{
    static constexpr std::size_t kChannels = 8;

    uint64_t timestamp_us = 0;
    std::array<uint16_t, kChannels> channels{};
    bool valid = false;
};

enum RcChannel : std::size_t
{
    kRcRoll = 0,
    kRcPitch = 1,
    kRcThrottle = 2,
    kRcYaw = 3,
    kRcArm = 4,
};

struct ActuatorOutputs
{
    static constexpr std::size_t kMaxMotors = 8;

    std::array<float, kMaxMotors> motors{};  ///< each in [0, 1]
    uint8_t count = 0;
    bool armed = false;
};

}  // namespace rfs::fc
