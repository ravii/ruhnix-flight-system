/**
 * Board implementation template. Replace every TODO with calls into the
 * vendor HAL in vendor/.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "esc_config.hpp"
#include "rfs/fc/bsp/board.hpp"
#include "rfs/fc/bsp/esc_bus.hpp"
#include "rfs/fc/bsp/rtos_time.hpp"
#include "rfs/fc/esc/esc_output.hpp"

namespace
{

/**
 * Motor signal outputs. Only generates waveforms: what to send is decided by
 * rfs::fc::EscOutput from config/esc_config.hpp.
 */
class TemplateEscBus final : public rfs::fc::EscBus
{
public:
    bool configure(
        rfs::fc::EscProtocol protocol, bool bidirectional, uint8_t channel_count) override
    {
        // TODO: one timer channel per output, in output-channel order.
        //   DShot:  bit period from dshot_bitrate(protocol); a DMA stream
        //           writes one compare value per bit (0 -> ~37 %, 1 -> ~75 % duty).
        //   analog: PWM at 400 Hz (kPwm) or ~2 kHz (kOneShot125), pulse widths
        //           from analog_pulse_range(protocol).
        //   Return false for anything the hardware cannot do (e.g. bidirectional).
        (void)protocol;
        (void)channel_count;
        return !bidirectional;
    }

    void send(const rfs::fc::EscFrame& frame) override
    {
        // TODO: DShot: expand each 16-bit frame (MSB first) into the DMA buffer
        // and start the burst. Analog: load frame.values[i] (ns) into CCR i.
        (void)frame;
    }
};

class TemplateBoard final : public rfs::fc::Board
{
public:
    void init() override
    {
        // TODO: clocks, GPIO, SPI (IMU), timers (motors), UART (link).
        // Configure the IMU data-ready EXTI to call on_imu_ready().

        // Keep side effects out of configASSERT: it may be compiled out.
        const bool esc_ok = esc_.init(rfs::fc::kEscConfig);
        configASSERT(esc_ok);
        (void)esc_ok;
    }

    uint64_t time_us() override
    {
        return 0;  // TODO: free-running 32-bit timer extended to 64 bits
    }

    bool wait_imu(rfs::fc::ImuSample& out, uint32_t timeout_ms) override
    {
        imu_task_ = xTaskGetCurrentTaskHandle();
        if (ulTaskNotifyTake(pdTRUE, rfs::fc::timeout_ticks(timeout_ms)) == 0)
        {
            return false;
        }
        out.timestamp_us = time_us();
        // TODO: read gyro/accel over SPI, convert to rad/s and m/s^2 in FRD.
        return true;
    }

    void read_rc(rfs::fc::RcInput& out) override
    {
        out.valid = false;  // TODO: SBUS/CRSF receiver
    }

    void write_motors(const rfs::fc::ActuatorOutputs& outputs) override { esc_.write(outputs); }

    std::size_t link_write(const uint8_t* data, std::size_t len) override
    {
        (void)data;  // TODO: UART DMA TX
        return len;
    }

    std::size_t link_read(uint8_t* data, std::size_t max_len) override
    {
        (void)data;  // TODO: UART DMA RX ring buffer
        (void)max_len;
        return 0;
    }

    /** Call from the IMU data-ready interrupt. */
    void on_imu_ready_isr()
    {
        BaseType_t woken = pdFALSE;
        if (imu_task_ != nullptr)
        {
            vTaskNotifyGiveFromISR(imu_task_, &woken);
        }
        portYIELD_FROM_ISR(woken);
    }

private:
    TaskHandle_t imu_task_ = nullptr;
    TemplateEscBus esc_bus_;
    rfs::fc::EscOutput esc_{esc_bus_};
};

TemplateBoard g_board;

}  // namespace

rfs::fc::Board& rfs::fc::board()
{
    return g_board;
}

// TODO: hook into the vendor EXTI handler, e.g.
// extern "C" void EXTI4_IRQHandler(void) { /* clear flag */ g_board.on_imu_ready_isr(); }
