/**
 * FreeRTOS tick conversions for board and application code (never core/).
 *
 * The tick is slow (configTICK_RATE_HZ = 50, 20 ms), which breaks two habits:
 *   - pdMS_TO_TICKS() rounds down: pdMS_TO_TICKS(10) == 0, "don't wait".
 *   - a block of N ticks ends at the N-th tick boundary, and the first one
 *     can come right after the call: it lasts between N-1 and N tick periods.
 * A timeout of 1 tick can therefore expire after a few microseconds, e.g.
 * before the next 1 kHz IMU sample. Use timeout_ticks() for timeouts.
 */
#pragma once

#include <cstdint>

#include "FreeRTOS.h"

namespace rfs::fc
{

/** Smallest block time that waits at least timeout_ms. 0 stays 0 (poll). */
constexpr TickType_t timeout_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return 0;
    }
    const uint64_t rate = configTICK_RATE_HZ;
    const uint64_t ticks = (uint64_t{timeout_ms} * rate + 999u) / 1000u + 1u;  // ceil, +1
    const uint64_t max_ticks = portMAX_DELAY - 1u;  // portMAX_DELAY means "forever"
    return static_cast<TickType_t>(ticks < max_ticks ? ticks : max_ticks);
}

static_assert(timeout_ticks(0) == 0, "zero timeout must poll");
static_assert(timeout_ticks(1) >= 2, "a timeout must never end at the next tick boundary");

}  // namespace rfs::fc
