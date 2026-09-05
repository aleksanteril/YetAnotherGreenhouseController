#ifndef SHELL_HPP
#define SHELL_HPP

#include "FreeRTOS.h"
#include "semphr.h"
#include "timers.h"
#include <array>

#include "metropolia_io.hpp"
#include "PicoOsUart.h"

class Shell {
      public:
        Shell();
        void 
        loop();

      private:
        void 
        print_usage();
        bool 
        read_cmd();
        void 
        parse_value(std::string_view& cmd);
        void 
        parse_cmd();
        static void
        inactivity_timer(TimerHandle_t);
        
        PicoOsUart u{ PicoOsUart(0 , UART0_TX_PIN, UART0_RX_PIN, 115200) };
        SemaphoreHandle_t semaph{ xSemaphoreCreateBinary() };
        TimerHandle_t inactivity_tmr{ xTimerCreate("INACTIVITY TIMER", 
                pdMS_TO_TICKS(30000), pdFALSE, (void*)semaph, inactivity_timer) };

        static constexpr char END{ '\r' };
        std::array<uint8_t, 64> buf;
        uint8_t* ptr{ buf.data() };
};

#endif
