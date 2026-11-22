/**
 * C++ runtime support for the firmware:
 *   - operator new/delete are routed to the FreeRTOS heap (heap_4)
 *   - assertion and FreeRTOS hook handlers
 *
 * Built with -fno-exceptions: allocation failure is fatal, never thrown.
 */
#include <cstddef>
#ifdef RFS_FC_SITL
#include <cstdio>
#include <cstdlib>
#endif

#include "FreeRTOS.h"
#include "task.h"

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

void fc_assert_failed(const char* file, int line)
{
#ifdef RFS_FC_SITL
    // SITL: a Linux process; report and die so the run fails visibly.
    std::fprintf(stderr, "flight_controller: assertion failed at %s:%d\n", file, line);
    std::abort();
#endif
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        // Attach a debugger here; file/line identify the failed assertion.
    }
}

void vApplicationMallocFailedHook(void)
{
    fc_assert_failed(__FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    (void)name;
    fc_assert_failed(__FILE__, __LINE__);
}

}  // extern "C"
