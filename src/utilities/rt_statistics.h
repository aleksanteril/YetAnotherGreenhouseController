#ifndef RT_STATISTICS_H
#define RT_STATISTICS_H

extern "C" 
{
        uint32_t read_runtime_ctr(void);
        void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName);
}

#endif
