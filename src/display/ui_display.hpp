#ifndef UI_DISPLAY_HPP
#define UI_DISPLAY_HPP

#include "FreeRTOS.h"
#include <memory>
#include "queue.h"
#include "ssd1306os.h"
#include "eeprom.hpp"
#include "timers.h"
#include <array>

enum Event { LEFT, RIGHT, INACTIVE, UPDATE, PRESS_SW0 = SW0_PIN, PRESS_SW2 = SW2_PIN };
extern QueueHandle_t DisplayEvent_q;

// We are the sole device on I2C1, this project doesn't use the pressure sensor
class UI_Display {
      public:
        UI_Display(EEPROM* e);
        UI_Display(const UI_Display&) = delete;
        void
        loop();

      private:
        enum intEvent { Enter, Exit, Loop };
        using State = void(UI_Display::*)(enum intEvent);
        void
        change_state_to(State);

        // States and their variables and helper funcs
        void
        status(enum intEvent);

        std::array<char, 16> info_setp;
        std::array<char, 24> info_ipv4;
        void
        info(enum intEvent);

        void
        wait(enum intEvent);

        void
        handle_draw_val();
        std::array<int, 4> set_val{};
        int res{};
        void
        set_ipv4(enum intEvent);
        void
        set_target(enum intEvent);
        
        int menu_i{};
        std::array<State, 3> items{ &UI_Display::info, 
                &UI_Display::set_target, &UI_Display::set_ipv4 };
        void
        menu(enum intEvent);
        
        // Starting state
        Event ev;
        State state{ &UI_Display::menu };

        std::shared_ptr<PicoI2C> i2cbus{std::make_shared<PicoI2C>(1, 400000)};
        ssd1306os display{i2cbus};
        QueueHandle_t event_q{ xQueueCreate(15, sizeof(Event)) };

        static void
        inactivity_timer(TimerHandle_t xTimer);
        TimerHandle_t inact_t{ xTimerCreate("INACT_TMR", pdMS_TO_TICKS(20000), 
                pdFALSE, nullptr, inactivity_timer) };

        EEPROM* eeprom;
};

#endif
