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

namespace fc::esc::dshot
{

constexpr uint16_t kValueMax = 2047;
constexpr uint16_t kThrottleMin = 48;  ///< lowest throttle; the motor spins (armed idle)
constexpr uint16_t kThrottleMax = 2047;

/**
 * Special commands (frame values 0..47). ESCs accept them only while the
 * motors are stopped, and settings commands only after several identical
 * frames: send them kCommandRepeat times in a row.
 */
enum class Command : uint8_t
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

constexpr int kCommandRepeat = 10;

/** Builds a frame. Values above kValueMax are clamped. */
uint16_t encode(uint16_t value, bool telemetry, bool bidirectional);

/**
 * Maps throttle [0, 1] to [kThrottleMin, kThrottleMax]. Out-of-range input
 * and NaN are clamped (NaN gives kThrottleMin). Never returns motor stop.
 */
uint16_t throttle_value(float throttle);

/** Frame for a command. All commands except kMotorStop set the telemetry bit. */
uint16_t command_frame(Command command, bool bidirectional);

}  // namespace fc::esc::dshot
