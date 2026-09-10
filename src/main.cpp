#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "task.h"

// this is needed for runtime statistics
#include "rt_statistics.h"

#include "shell.hpp"
#include "ui_display.hpp"

#include "ModbusClient.h"
#include "sensors.hpp"
#include "cmath"

void
sensor_task(void* param);
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

        /*
        xTaskCreate(shell_task, "SHELL", 
                312, (void*)&e, tskIDLE_PRIORITY + 1, NULL);
        xTaskCreate(ui_task, "UI", 
                512, (void*)&e, tskIDLE_PRIORITY + 1, NULL);
        */
        
        xTaskCreate(sensor_task, "SENSORS", 
                1024, (void*)&e, tskIDLE_PRIORITY + 1, NULL);

        printf("Start FreeRTOS scheduler\n");
        vTaskStartScheduler();
}

constexpr int SETPOINT{ 1500 };
constexpr double kp{ 0.075 };

constexpr uint UART1_BAUD{ 9600 };
constexpr uint UART1_TX_PIN{ 4 };
constexpr uint UART1_RX_PIN{ 5 };

void
sensor_task(void* param)
{
        auto uart_ptr{ std::make_shared<PicoOsUart>(1, 
                UART1_TX_PIN, UART1_RX_PIN, UART1_BAUD, 2) };
        auto rtu{std::make_shared<ModbusClient>(uart_ptr)};
        GMP252 co2(rtu);
        HMP60 hmp(rtu);
        PRODUAL fan(rtu);
        CO2_Valve valve;
        fan.write_speed(800);
        // make a PI controller for co2 setpoint and valve and fan control
        for(;;) {
                int error = SETPOINT - co2.read();
                int p = std::round(kp*error);
                int output = p;
                valve.set_cycle(output);
                printf("out: %d\n", output);
                vTaskDelay(pdMS_TO_TICKS(2000));
        }
}

void
ui_task(void* param)
{
        
        UI_Display ui(*(EEPROM*)param);
        for(;;) {
                // Display sleeps unless events come on queue
                ui.loop();
        }
}

void
shell_task(void* param)
{       
        Shell sh(*(EEPROM*)param);
        for(;;) {
                // Task sleeps until activity starts on UART0
                sh.loop();
        }
}
