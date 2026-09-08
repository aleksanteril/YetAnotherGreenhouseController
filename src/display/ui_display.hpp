#ifndef UI_DISPLAY_HPP
#define UI_DISPLAY_HPP

#include "FreeRTOS.h"
#include <memory>
#include "queue.h"
#include "ssd1306os.h"
#include "eeprom.hpp"
#include "timers.h"

enum Event { NONE, LEFT, RIGHT, PRESS, INACTIVE };

// We are the sole device on I2C1, this project doesn't use the pressure sensor
class UI_Display {
      public:
        UI_Display(EEPROM* e);
        UI_Display(const UI_Display&) = delete;
        void
        loop();
      private:
        enum State { STATUS, MENU };
        State state{ STATUS };

        std::shared_ptr<PicoI2C> i2cbus{std::make_shared<PicoI2C>(1, 400000)};
        ssd1306os display{i2cbus};
        QueueHandle_t event_q{ xQueueCreate(10, sizeof(Event)) };

        static void
        inactivity_timer(TimerHandle_t xTimer);
        TimerHandle_t inact_t{ xTimerCreate("INACT_TMR", pdMS_TO_TICKS(10000), 
                pdFALSE, (void*)&event_q, inactivity_timer) };

        EEPROM* e;
};

#endif
