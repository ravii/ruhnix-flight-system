/**
 * Board implementation template. Replace every TODO with calls into the
 * vendor HAL in vendor/.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "fc/bsp/board.hpp"

namespace
{

class TemplateBoard final : public fc::Board
{
public:
    void init() override
    {
        // TODO: clocks, GPIO, SPI (IMU), timers (motors), UART (link).
        // Configure the IMU data-ready EXTI to call on_imu_ready().
    }

    uint64_t time_us() override
    {
        return 0;  // TODO: free-running 32-bit timer extended to 64 bits
    }

    bool wait_imu(fc::ImuSample& out, uint32_t timeout_ms) override
    {
        imu_task_ = xTaskGetCurrentTaskHandle();
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0)
        {
            return false;
        }
        out.timestamp_us = time_us();
        // TODO: read gyro/accel over SPI, convert to rad/s and m/s^2 in FRD.
        return true;
    }

    void read_rc(fc::RcInput& out) override
    {
        out.valid = false;  // TODO: SBUS/CRSF receiver
    }

    void write_motors(const fc::ActuatorOutputs& outputs) override
    {
        (void)outputs;  // TODO: PWM/DShot
    }

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
};

TemplateBoard g_board;

}  // namespace

fc::Board& fc::board()
{
    return g_board;
}

// TODO: hook into the vendor EXTI handler, e.g.
// extern "C" void EXTI4_IRQHandler(void) { /* clear flag */ g_board.on_imu_ready_isr(); }
