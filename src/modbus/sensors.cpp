#include "sensors.hpp"
#include "hardware/gpio.h"
#include "pico/time.h"

// Sensor values mailboxes that can be peeked to read current val.
QueueHandle_t Sensors::rh_mb{ nullptr };
QueueHandle_t Sensors::temp_mb{ nullptr };
QueueHandle_t Sensors::co2_mb{ nullptr };

using namespace Sensors;

// humidity and temp sensor
constexpr uint HMP60_ADDR{ 241 };
HMP60::HMP60(std::shared_ptr<ModbusClient> client_)
: rh(client_, HMP60_ADDR, 256), temp(client_, HMP60_ADDR, 257) 
{
        rh_mb = xQueueCreate(1, sizeof(SensorPacket_t));
        vQueueAddToRegistry(rh_mb, "RH_MB");
        temp_mb = xQueueCreate(1, sizeof(SensorPacket_t));
        vQueueAddToRegistry(temp_mb, "TEMP_MB");
}

double
HMP60::read_rh() 
{ 
        double val{ (rh.read() / 10.0) }; 
        Sensors::SensorPacket_t packet{
                .timestamp = xTaskGetTickCount(),
                .doubl = val
        };
        xQueueOverwrite(rh_mb, (void*)&packet);
        return val;
}
double
HMP60::read_temp() 
{ 
        double val{ (temp.read() / 10.0) }; 
        Sensors::SensorPacket_t packet{
                .timestamp = xTaskGetTickCount(),
                .doubl = val
        };
        xQueueOverwrite(temp_mb, (void*)&packet);
        return val;
}

// co2 sensor
constexpr uint GMP252_ADDR{ 240 };
GMP252::GMP252(std::shared_ptr<ModbusClient> client_) 
: co2(client_, GMP252_ADDR, 256) 
{
        co2_mb = xQueueCreate(1, sizeof(SensorPacket_t));
        vQueueAddToRegistry(co2_mb, "CO2_MB");
}

uint16_t
GMP252::read_co2()
{
        uint16_t val{ co2.read() };
        Sensors::SensorPacket_t packet{
                .timestamp = xTaskGetTickCount(),
                .uint16 = val
        };
        xQueueOverwrite(co2_mb, (void*)&packet);
        return val;
}


// fan control
constexpr uint PRODUAL_ADDR{ 1 };
PRODUAL::PRODUAL(std::shared_ptr<ModbusClient> client_)
: counter(client_, PRODUAL_ADDR, 4, false), speed(client_, PRODUAL_ADDR, 0) 
{
}

int
PRODUAL::read_counter() 
{ 
        return counter.read(); 
}

double
PRODUAL::read_speed() 
{ 
        return (speed.read() / 10.0); 
}

bool
PRODUAL::watchdog()
{
        if(running) {
                //set running and counter comes back 0 fail
                return (bool)read_counter();
        } else {
                //not running just ignore
                return true;
        }
}

// 0 - 1000
void
PRODUAL::write_speed(uint16_t val) 
{ 
        if(!running && val > 0) {
                // kickstart to run
                speed.write(600);
                vTaskDelay(pdMS_TO_TICKS(10));
                running = true;
        } else if(running && val <= 0) {
                speed.write(0);
                running = false;
        }
        if(running) {
                speed.write(val > 1000 ? 1000 : val);
        }
}

void
PRODUAL::set_output(int percent)
{
        if(percent < 0) {
                percent = 0;
        } else if(percent < 20) {
                // minimum speed to turn the fan
                percent = 20;
        } else if(percent > 100) {
                percent = 100;
        }
        write_speed(percent*10);
}


// co2 valve control
static int64_t 
valve_shut_isr(alarm_id_t id, void* user_data) {
        auto* valve{ (CO2_Valve*)user_data };
        valve->close();
        return 0;
}

constexpr uint CO2_VALVE_PIN{ 27 };
constexpr uint LED1_PIN{ 20 };
CO2_Valve::CO2_Valve() 
: pin(CO2_VALVE_PIN), led_pin(LED1_PIN)
{
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, false);

        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);
        gpio_put(led_pin, false);
}

void 
CO2_Valve::open() 
{ 
        gpio_put(pin, true); 
        gpio_put(led_pin, true);
}
void
CO2_Valve::close() 
{ 
        gpio_put(pin, false); 
        gpio_put(led_pin, false);
}

// set 0% - 100%
void
CO2_Valve::set_output(int percent)
{
        if(percent <= 0) {
                xTimerStop(tmr, 0);
                //close();
                return;
        } 
        if(percent < 5) {
                // min 1s of co2
                percent = 5;
        } else if(percent > 30) {
                // max 3s of co2
                percent = 30;
        }
        open_ms = percent*100;
        if(xTimerIsTimerActive(tmr) == pdFALSE) {
                xTimerStart(tmr, 0);
        }
}

// Opens the valve and sets a ISR timer to shut it
void
CO2_Valve::valve_timer(TimerHandle_t xTimer)
{
        auto* valve{ (CO2_Valve*)pvTimerGetTimerID(xTimer) };
        valve->open();
        add_alarm_in_ms(valve->open_ms, valve_shut_isr, (void*)valve, true);
}

