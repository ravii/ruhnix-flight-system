/*
 * FreeRTOS configuration for the flight controller.
 * See https://www.freertos.org/a00110.html for every option.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* clang-format off: keep the configuration table column-aligned. */

#include <stdint.h>

/*
 * Two targets share this file:
 *   - MCU boards (Cortex-M ports): the default.
 *   - SITL (bsp/sitl, FreeRTOS POSIX port): RFS_FC_SITL is defined by
 *     bsp/sitl/board.cmake. Each task is a pthread; see bsp/sitl/sitl_board.cpp.
 */

/* Included from both the C kernel and C++ application code. */
#ifdef __cplusplus
extern "C" {
#endif
#ifndef RFS_FC_SITL
extern uint32_t SystemCoreClock; /* provided by the board's system.c */
#endif
void fc_assert_failed(const char* file, int line);
#ifdef __cplusplus
}
#endif

/* Scheduler */
/*
 * Tick: 50 Hz (20 ms). The control loop does not use the tick: it is paced by
 * Board::wait_imu() (IMU interrupt / lockstep packet). The tick only drives
 * vTaskDelay(), software timers, block timeouts and time slicing, all with
 * 20 ms resolution. pdMS_TO_TICKS() rounds down (pdMS_TO_TICKS(10) == 0):
 * convert timeouts with rfs::fc::timeout_ticks() (rfs/fc/bsp/rtos_time.hpp).
 */
#define configUSE_PREEMPTION 1
#define configUSE_TICKLESS_IDLE 0
#define configTICK_RATE_HZ ((TickType_t)50)
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE ((uint16_t)128)
#define configMAX_TASK_NAME_LEN 16
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TIME_SLICING 1

/* Memory: kernel objects are created at init; no allocation after start. */
#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configTOTAL_HEAP_SIZE ((size_t)(32 * 1024))

/* Features */
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configQUEUE_REGISTRY_SIZE 8
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 256

/* Hooks and debugging */
#ifdef RFS_FC_SITL
#define configUSE_IDLE_HOOK 1 /* SITL: the idle task waits for UDP packets */
#else
#define configUSE_IDLE_HOOK 0
#endif
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 0

#define configASSERT(x)                           \
    do                                            \
    {                                             \
        if (!(x))                                 \
        {                                         \
            fc_assert_failed(__FILE__, __LINE__); \
        }                                         \
    } while (0)

#ifdef RFS_FC_SITL

/* POSIX port: no optimised task selection, no CPU clock, no NVIC. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

#else

#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configCPU_CLOCK_HZ (SystemCoreClock)

/* Cortex-M interrupt priorities (STM32: 4 priority bits). */
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Map FreeRTOS port handlers onto CMSIS vector names. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* RFS_FC_SITL */

/* Optional API */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

/* clang-format on */

#endif /* FREERTOS_CONFIG_H */
