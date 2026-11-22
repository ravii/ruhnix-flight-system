/**
 * Hardware abstraction for the flight controller.
 *
 * The application (app/) talks to hardware only through rfs::fc::Board. Each
 * directory under bsp/ provides one implementation:
 *   bsp/<board>/  real hardware (SPI IMU, timers, UART ...)
 *   bsp/sim/      simulator: sensor data and time come from a physics model
 *
 * Timing contract: the control loop is paced by wait_imu(), not by an RTOS
 * timer. On hardware it returns on the IMU data-ready interrupt; in the
 * simulator it returns when the next lockstep sensor packet arrives. This is
 * what lets the unmodified firmware run in lockstep with a simulator.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "rfs/fc/types.hpp"

namespace rfs::fc
{

class Board
{
public:
    /** Clocks, GPIO and peripherals. Called once before the scheduler starts. */
    virtual void init() = 0;

    /** Monotonic time in microseconds. Simulation time when simulated. */
    virtual uint64_t time_us() = 0;

    /**
     * Blocks until a new IMU sample is available, then returns it.
     * @return false on timeout (sensor or simulator link lost).
     */
    virtual bool wait_imu(ImuSample& out, uint32_t timeout_ms) = 0;

    /** Optional sensors: return true only when a new sample was read. */
    virtual bool read_baro(BaroSample& /*out*/) { return false; }
    virtual bool read_mag(MagSample& /*out*/) { return false; }
    virtual bool read_gnss(GnssSample& /*out*/) { return false; }

    /** Latest RC input; out.valid == false on signal loss. */
    virtual void read_rc(RcInput& out) = 0;

    virtual void write_motors(const ActuatorOutputs& outputs) = 0;

    /** Byte stream to the companion computer (UART on hardware, UDP in SITL). Non-blocking. */
    virtual std::size_t link_write(const uint8_t* data, std::size_t len) = 0;
    virtual std::size_t link_read(uint8_t* data, std::size_t max_len) = 0;

    virtual void led_toggle() {}

protected:
    // Boards are never deleted through this interface (no heap on target).
    ~Board() = default;
};

/** Returns the board instance. Defined by the BSP selected with -DFC_BOARD. */
Board& board();

}  // namespace rfs::fc
