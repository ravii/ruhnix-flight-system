/** Flight controller entry point: board init, task creation, scheduler start. */
#include "FreeRTOS.h"
#include "task.h"

#include "control_task.hpp"
#include "fc/bsp/board.hpp"
#include "fc/flight_control.hpp"
#include "telemetry_task.hpp"

int main()
{
    fc::Board& board = fc::board();
    board.init();

    static fc::FlightControl control;
    static app::ControlTask control_task(board, control);
    static app::TelemetryTask telemetry_task(board);

    // Keep side effects out of configASSERT: it may be compiled out.
    const bool started = control_task.start("control", 1024, configMAX_PRIORITIES - 1) &&
                         telemetry_task.start("telemetry", 512, tskIDLE_PRIORITY + 1);
    configASSERT(started);
    (void)started;

    vTaskStartScheduler();
    for (;;)
    {
        // Only reached if the scheduler failed to start (out of heap).
    }
}
