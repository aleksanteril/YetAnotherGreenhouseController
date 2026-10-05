#ifndef SENSORS_HPP
#define SENSORS_HPP

#include "FreeRTOS.h"
#include "ModbusRegister.h"
#include "timers.h"
#include <cmath>

namespace Sensors
{
        struct SensorPacket_t {
                TickType_t timestamp;
                union {
                        double f64;
                        uint16_t u16;
                };
        };

        // Sensor values mailboxes that can be peeked to read current val.
        extern QueueHandle_t rh_mb;
        extern QueueHandle_t temp_mb;
        extern QueueHandle_t co2_mb;
}


// humidity and temp sensor
class HMP60 {
      public:
        explicit HMP60(std::shared_ptr<ModbusClient> client_);
        HMP60(const HMP60&) = delete;
        double
        read_rh();
        double
        read_temp();

      private:
        ModbusRegister rh;
        ModbusRegister temp;
};

// co2 sensor
class GMP252 {
      public:
        explicit GMP252(std::shared_ptr<ModbusClient> client_);
        GMP252(const GMP252&) = delete;
        uint16_t
        read_co2();
      private:
        ModbusRegister co2;
};

// fan control
class PRODUAL {
      public:
        explicit PRODUAL(std::shared_ptr<ModbusClient> client_);
        PRODUAL(const PRODUAL&) = delete;
        int
        read_counter();
        double
        read_speed();
        // 0 - 1000
        void
        write_speed(uint16_t val);
        void
        set_output(int percent);

      private:
        bool running{ false };
        ModbusRegister counter;
        ModbusRegister speed;
};

// co2 valve control
class CO2_Valve {
      public:
        CO2_Valve();
        CO2_Valve(const PRODUAL&) = delete;
        void 
        open();
        void
        close();
        // set 0% - 100%
        void
        set_output(int percent);

      private:
        const uint pin;
        const uint led_pin;
        uint32_t open_ms{ 1000 };

        const TimerHandle_t tmr{ xTimerCreate("CO2 VALVE", 
                pdMS_TO_TICKS(30000), pdTRUE, (void*)this, valve_timer) };
        // Opens the valve and sets a ISR timer 1s to shut it
        static void
        valve_timer(TimerHandle_t xTimer);
};


// P-controller (PI potential)
template <typename Device>
class P_Controller {
      public:
        // dir, true(default) = direct action, false = reverse action
        P_Controller(Device& d, int sp, double kp, bool dir=true)
        : d(d), sp(sp), kp(dir ? kp : (kp*-1)) 
        {
        }
        void
        loop(int pv)
        {
                int error{ sp - pv };
                int output{ (int)std::round(kp*error) };
                d.set_output(output);
        }
        int sp;

      private:
        Device& d;
        const double kp;
};

#endif
