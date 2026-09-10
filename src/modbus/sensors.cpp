#include "sensors.hpp"
#include "hardware/gpio.h"
#include "pico/time.h"

// humidity and temp sensor
constexpr uint HMP60_ADDR{ 241 };
HMP60::HMP60(std::shared_ptr<ModbusClient> client_)
: rh(client_, HMP60_ADDR, 256), temp(client_, HMP60_ADDR, 257) 
{
}

double
HMP60::read_rh() 
{ 
        return (rh.read() / 10.0); 
}
double
HMP60::read_temp() 
{ 
        return (temp.read() / 10.0); 
}

// co2 sensor
constexpr uint GMP252_ADDR{ 240 };
GMP252::GMP252(std::shared_ptr<ModbusClient> client_) 
: ModbusRegister(client_, GMP252_ADDR, 256) 
{

}


// fan control
constexpr uint PRODUAL_ADDR{ 1 };
PRODUAL::PRODUAL(std::shared_ptr<ModbusClient> client_)
: wd(client_, PRODUAL_ADDR, 0, false), speed(client_, PRODUAL_ADDR, 0) 
{
}

int
PRODUAL::read_wd() 
{ 
        return wd.read(); 
}

double
PRODUAL::read_speed() 
{ 
        return (speed.read() / 10.0); 
}

// 0 - 1000
void
PRODUAL::write_speed(uint16_t val) 
{ 
        speed.write(val > 1000 ? 1000 : val);
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
        auto valve{ (CO2_Valve*)user_data };
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
                close();
                return;
        } else if(percent > 100) {
                percent = 100;
        }
        // allow max 10s of valve continuously open
        open_ms = percent*100;
        if(xTimerIsTimerActive(tmr) == pdFALSE) {
                xTimerStart(tmr, 0);
        }
}

// Opens the valve and sets a ISR timer to shut it
void
CO2_Valve::valve_timer(TimerHandle_t xTimer)
{
        auto valve{ (CO2_Valve*)pvTimerGetTimerID(xTimer) };
        valve->open();
        add_alarm_in_ms(valve->open_ms, valve_shut_isr, (void*)valve, true);
}

