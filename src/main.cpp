#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "task.h"
#include "queue.h"

// this is needed for runtime statistics
#include "rt_statistics.h"

#include "shell.hpp"
#include "ui_display.hpp"
#include "ModbusClient.h"
#include "sensors.hpp"

void
controller_task(void* param);
void
temp_rh_task(void* param);
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
                392, (void*)&e, tskIDLE_PRIORITY + 1, NULL);
        xTaskCreate(ui_task, "UI", 
                320, (void*)&e, tskIDLE_PRIORITY + 1, NULL);
        xTaskCreate(controller_task, "CONTROLLER", 
                256, (void*)&e, tskIDLE_PRIORITY + 2, NULL);

        printf("Start FreeRTOS scheduler\n");
        vTaskStartScheduler();
}

void
temp_rh_task(void* param)
{
        auto rtu{ *(std::shared_ptr<ModbusClient>*)param };
        HMP60 hmp(rtu);
        int time_ms{ 60'000 }; // 1minute

        Display::Event updt{ Display::UPDATE };
        hmp.read_rh();
        hmp.read_temp();
        for(;;) {
                vTaskDelay(pdMS_TO_TICKS(time_ms));
                hmp.read_rh();
                hmp.read_temp();
                xQueueSend(Display::Event_q, &updt, 0);
        }
}

constexpr uint UART1_BAUD{ 9600 };
constexpr uint UART1_TX_PIN{ 4 };
constexpr uint UART1_RX_PIN{ 5 };

void
controller_task(void* param)
{
        auto uart_ptr{ std::make_shared<PicoOsUart>(1, 
                UART1_TX_PIN, UART1_RX_PIN, UART1_BAUD, 2) };
        auto rtu{std::make_shared<ModbusClient>(uart_ptr)};
        // spin-off temp and rh measuring to another task
        // as this needs to be run much less frequently
        xTaskCreate(temp_rh_task, "TEMP & RH", 
                128, (void*)&rtu, tskIDLE_PRIORITY + 1, nullptr);
        
        GMP252 gmp(rtu);
        PRODUAL fan(rtu);
        CO2_Valve valve;
        P_Controller pid_fan(fan, 2000, 2, false);
        int sp{};
        xQueuePeek(EEPROMCache::setpoint_mb, &sp, 0);
        P_Controller pid_valve(valve, sp, 0.15);

        Display::Event updt{ Display::UPDATE };
        for(;;) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                int new_sp{};
                xQueuePeek(EEPROMCache::setpoint_mb, &new_sp, 0);
                sp = (new_sp != sp) ? (pid_valve.sp = new_sp) : sp;
                uint16_t co2_value{ gmp.read_co2() };
                pid_valve.loop(co2_value);
                pid_fan.loop(co2_value);
                xQueueSend(Display::Event_q, &updt, 0);
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
        // hack, 
        // make shell responsible for loading setpoint from EEPROM if possible
        // this is due to the i2c library functions not working in main 
        // (outside of tasks etc.)
        auto* e{ (EEPROM*)param };
        uint16_t dummy;
        e->load_setpoint(dummy);
        Shell sh(*e);
        for(;;) {
                // Task sleeps until activity starts on UART0
                sh.loop();
        }
}
