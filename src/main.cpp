#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "task.h"

// this is needed for runtime statistics
#include "rt_statistics.h"

#include "shell/shell.hpp"

void
shell_task(void* param);

int 
main()
{
        stdio_init_all();
        printf("\nBoot\n");

        xTaskCreate(shell_task, "SHELL", 
                512, NULL, tskIDLE_PRIORITY + 1, NULL);

        printf("Start FreeRTOS scheduler\n");
        vTaskStartScheduler();
}

void
shell_task(void* param)
{       
        Shell sh;
        for(;;) {
                sh.loop();
        }
}
