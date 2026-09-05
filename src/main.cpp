#include "FreeRTOS.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "hardware/sync.h"
#include "task.h"
#include "semphr.h"
#include "metropolia_io.hpp"
#include "PicoOsUart.h"
#include "timers.h"
#include <cstdio>
#include <string.h>

// this is needed for runtime statistics
#include "rt_statistics.h"

int 
main()
{
        stdio_init_all();
        printf("\nBoot\n");

        printf("Start FreeRTOS scheduler\n");
        vTaskStartScheduler();
}
