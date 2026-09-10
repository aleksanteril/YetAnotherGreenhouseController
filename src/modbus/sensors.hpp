#ifndef SENSORS_HPP
#define SENSORS_HPP

#include "FreeRTOS.h"
#include "ModbusRegister.h"
#include "timers.h"

struct SensorVal_t {
        TickType_t timestamp;
        union value {
                double doubl;
                uint16_t uint16;
        };
};

// humidity and temp sensor
class HMP60 {
      public:
        HMP60(std::shared_ptr<ModbusClient> client_);
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
class GMP252 : public ModbusRegister {
      public:
        GMP252(std::shared_ptr<ModbusClient> client_);
        GMP252(const GMP252&) = delete;
};

// fan control
class PRODUAL {
      public:
        PRODUAL(std::shared_ptr<ModbusClient> client_);
        PRODUAL(const PRODUAL&) = delete;
        int
        read_wd();
        double
        read_speed();
        // 0 - 1000
        void
        write_speed(uint16_t val);

      private:
        ModbusRegister wd;
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
        set_cycle(int percent);

      private:
        const uint pin;
        const uint led_pin;
        uint32_t open_ms{ 1000 };

        TimerHandle_t tmr{ xTimerCreate("CO2 VALVE", 
                pdMS_TO_TICKS(30000), pdTRUE, (void*)this, valve_timer) };
        // Opens the valve and sets a ISR timer 1s to shut it
        static void
        valve_timer(TimerHandle_t xTimer);
};

#endif
