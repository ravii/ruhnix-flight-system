/** Thin C++ wrapper over a FreeRTOS task. Derive, implement run(), call start(). */
#pragma once

#include "FreeRTOS.h"
#include "task.h"

namespace rfs::fc
{

class Task
{
public:
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool start(const char* name, configSTACK_DEPTH_TYPE stack_words, UBaseType_t priority)
    {
        return xTaskCreate(&Task::entry, name, stack_words, this, priority, &handle_) == pdPASS;
    }

    TaskHandle_t handle() const { return handle_; }

protected:
    Task() = default;
    ~Task() = default;  // tasks are static objects, never deleted

    /** Task body. Must not return (if it does, the task deletes itself). */
    virtual void run() = 0;

private:
    static void entry(void* self)
    {
        static_cast<Task*>(self)->run();
        vTaskDelete(nullptr);
    }

    TaskHandle_t handle_ = nullptr;
};

}  // namespace rfs::fc
