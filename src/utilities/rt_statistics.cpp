
// this is needed for runtime statistics
#include "FreeRTOS.h"
#include "hardware/timer.h"
#include "task.h"
#include "rt_statistics.h"

extern "C"
{
        uint32_t read_runtime_ctr(void)
        {
                return timer_hw->timerawl;
        }


        // stack overflow check
        void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
        {
                if (pcTaskName != NULL)
                        panic("Stack overflow: %s", pcTaskName);
                else
                        panic("Stack overflow of unnamed task");
        }
}
