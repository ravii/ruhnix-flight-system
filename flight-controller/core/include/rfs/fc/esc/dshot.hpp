/**
 * DShot frame encoding.
 *
 *   bit 15..5  value: 0 = motor stop, 1..47 = command, 48..2047 = throttle
 *   bit 4      telemetry request
 *   bit 3..0   CRC = XOR of the three nibbles of bits 15..4
 *              (inverted for bidirectional DShot)
 */
#pragma once

#include <cstdint>

namespace rfs::fc
{

constexpr uint16_t kDshotValueMax = 2047;
constexpr uint16_t kDshotThrottleMin = 48;  ///< lowest throttle; the motor spins (armed idle)
constexpr uint16_t kDshotThrottleMax = 2047;

/**
 * Special commands (frame values 0..47). ESCs accept them only while the
 * motors are stopped, and settings commands only after several identical
 * frames: send them kDshotCommandRepeat times in a row.
 */
enum class DshotCommand : uint8_t
{
    kMotorStop = 0,
    kBeep1 = 1,
    kBeep2 = 2,
    kBeep3 = 3,
    kBeep4 = 4,
    kBeep5 = 5,
    kEscInfo = 6,
    kSpinDirection1 = 7,
    kSpinDirection2 = 8,
    k3dModeOff = 9,
    k3dModeOn = 10,
    kSaveSettings = 12,
    kExtendedTelemetryEnable = 13,
    kExtendedTelemetryDisable = 14,
    kSpinDirectionNormal = 20,
    kSpinDirectionReversed = 21,
};

constexpr int kDshotCommandRepeat = 10;

/** Builds a frame. Values above kDshotValueMax are clamped. */
uint16_t dshot_encode(uint16_t value, bool telemetry, bool bidirectional);

/**
 * Maps throttle [0, 1] to [kDshotThrottleMin, kDshotThrottleMax]. Out-of-range input
 * and NaN are clamped (NaN gives kDshotThrottleMin). Never returns motor stop.
 */
uint16_t dshot_throttle_value(float throttle);

/** Frame for a command. All commands except kMotorStop set the telemetry bit. */
uint16_t dshot_command_frame(DshotCommand command, bool bidirectional);

}  // namespace rfs::fc
