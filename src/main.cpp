#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "task.h"

// this is needed for runtime statistics
#include "rt_statistics.h"

#include "shell.hpp"
#include "ui_display.hpp"

void
shell_task(void* param);
void
ui_task(void* param);

int 
main()
{
        stdio_init_all();
        printf("\nBoot\n");
        
        // Only device @ 0x50 on i2c0
        static EEPROM e;

        xTaskCreate(shell_task, "SHELL", 
                312, (void*)&e, tskIDLE_PRIORITY + 1, NULL);
        xTaskCreate(ui_task, "UI", 
                512, (void*)&e, tskIDLE_PRIORITY + 1, NULL);

        printf("Start FreeRTOS scheduler\n");
        vTaskStartScheduler();
}

void
ui_task(void* param)
{
        
        UI_Display ui((EEPROM*)param);
        for(;;) {
                ui.loop();
        }
}

void
shell_task(void* param)
{       
        Shell sh((EEPROM*)param);
        for(;;) {
                // Task sleeps until activity starts on UART0
                sh.loop();
        }
}
