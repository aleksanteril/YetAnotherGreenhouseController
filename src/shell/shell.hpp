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
        int 
        read_chars();
        void 
        parse_value(std::string_view& cmd);
        void 
        parse_cmd();
        
        PicoOsUart u{ PicoOsUart(0 , UART0_TX_PIN, UART0_RX_PIN, 115200) };
        std::array<uint8_t, 64> buf;
        uint8_t* ptr{ buf.data() };
};

#endif
