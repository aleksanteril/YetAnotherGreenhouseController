#include "FreeRTOS.h"
#include "pico/stdio.h"
#include "task.h"
#include "queue.h"
#include "IPStack.h"

// this is needed for runtime statistics
#include "rt_statistics.h"

#include "shell.hpp"
#include "ui_display.hpp"
#include "ModbusClient.h"
#include "sensors.hpp"
#include "ThingSpeakClient.h"
#include "constants.hpp"

void
controller_task(void* param);
void
temp_rh_task(void* param);
void
shell_task(void* param);
void
ui_task(void* param);
void
network_task(void* param);

void
startup_task(void* param)
{
        // this task is needed as eeprom i2c dependant libraries cant be called
        // outside of the scheduler running.

        // Only device @ 0x50 on i2c0
        // constructor fills EEPROMCache mb's with default values
        static EEPROM e; 
        {
                uint16_t dummy;
                e.load_setpoint(dummy);
        }
        {
                Cache::NetworkSetting_t dummy;
                e.load_network(dummy);
        }

        xTaskCreate(shell_task, "SHELL",
                392, (void*)&e, tskIDLE_PRIORITY + 1, nullptr);
        xTaskCreate(ui_task, "UI", 
                320, (void*)&e, tskIDLE_PRIORITY + 1, nullptr);
        xTaskCreate(controller_task, "CONTROLLER", 
                256, (void*)&e, tskIDLE_PRIORITY + 2, nullptr);
        xTaskCreate(network_task, "NETWORK",
                2048, nullptr, tskIDLE_PRIORITY + 1, NULL);

        vTaskDelete(nullptr);
}

int 
main()
{
        stdio_init_all();
        printf("\nBoot\n");

        xTaskCreate(startup_task, "STARTUP",
                512, nullptr, tskIDLE_PRIORITY + 1, nullptr);

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

        int sp{};
        xQueuePeek(Cache::setpoint_mb, &sp, 0);
        P_Controller pid_valve(valve, sp, 0.1);
        bool fan_enable{};
        P_Controller pid_fan(fan, sp, 2, false);
        Display::Event updt{ Display::UPDATE };
        for(;;) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                int new_sp{};
                xQueuePeek(Cache::setpoint_mb, &new_sp, 0);
                if(new_sp != sp) {
                        pid_valve.sp = new_sp;
                        pid_fan.sp = new_sp;
                        sp = new_sp;
                }
                uint16_t co2{ gmp.read_co2() };
                if(co2 == 0) {
                        // co2 value 0 indicates error
                        // go into error state, dont run valve, fan on?.
                        valve.set_output(0);
                        fan.set_output(0);
                } else {
                        pid_valve.loop(co2);
                        if(fan_enable || co2 >= CO2_HLIMIT) {
                                pid_fan.loop(co2);
                                fan_enable = (co2 > sp);
                        }
                }
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
        Shell sh(*(EEPROM*)param);
        for(;;) {
                // Task sleeps until activity starts on UART0
                sh.loop();
        }
}

void network_task(void *param) {

        Cache::NetworkSetting_t wlan;
        xQueuePeek(Cache::network_mb, &wlan, portMAX_DELAY);
        IPStack ipstack(wlan.ssid, wlan.pwd);

        ThingSpeakConfig cfg {
                .ssid = wlan.ssid,
                .password = wlan.pwd
        };
        ThingSpeakClient client(ipstack, cfg, cfg.server_ip, 80);
        for(;;) {
                // send every 30s
                vTaskDelay(pdMS_TO_TICKS(30'000));

                // build data packet, peek mailboxes
                ThingSpeakPacket data;
                Sensors::SensorPacket_t packet;
                xQueuePeek(Sensors::co2_mb, &packet, 0);
                data.co2_ppm = packet.u16;
                xQueuePeek(Sensors::temp_mb, &packet, 0);
                data.temperature = packet.f64;
                xQueuePeek(Sensors::rh_mb, &packet, 0);
                data.humidity = packet.f64;
                xQueuePeek(Cache::setpoint_mb, &data.setpoint_ppm, 0);
                data.fan_percent = 0; // right now unused

                // Push measurements to the cloud.
                if (!client.send_update(data)) {
                        // try to reconnect
                        while(!client.reconnect()) {
                                // retry every 1 min
                                vTaskDelay(pdMS_TO_TICKS(60'000));
                        }
                }
        }
}
