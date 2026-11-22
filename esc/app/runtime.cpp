/**
 * C++ runtime support for the firmware:
 *   - operator new/delete are routed to the FreeRTOS heap (heap_4)
 *   - assertion and FreeRTOS hook handlers
 *
 * Built with -fno-exceptions: allocation failure is fatal, never thrown.
 */
#include <cstddef>

#include "FreeRTOS.h"
#include "task.h"

#include "esc/bsp/board.hpp"

void* operator new(std::size_t size)
{
    void* p = pvPortMalloc(size);
    configASSERT(p != nullptr);
    return p;
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void* p) noexcept
{
    vPortFree(p);
}

void operator delete[](void* p) noexcept
{
    vPortFree(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    vPortFree(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
    vPortFree(p);
}

extern "C" {

void esc_assert_failed(const char* file, int line)
{
    (void)file;
    (void)line;
    esc::board().enable_pwm(false);  // fail safe: power stage off
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        // Attach a debugger here; file/line identify the failed assertion.
    }
}

void vApplicationMallocFailedHook(void)
{
    esc_assert_failed(__FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    (void)name;
    esc_assert_failed(__FILE__, __LINE__);
}

}  // extern "C"
