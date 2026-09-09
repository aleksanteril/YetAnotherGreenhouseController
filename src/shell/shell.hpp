#ifndef SHELL_HPP
#define SHELL_HPP

#include "FreeRTOS.h"
#include <array>

#include "metropolia_io.hpp"
#include "PicoOsUart.h"
#include "eeprom.hpp"

class Shell {
      public:
        Shell(EEPROM*);
        Shell(const Shell&) = delete;
        void 
        loop();

      private:
        void 
        print_usage();
        void
        print_settings();
        int 
        read_chars();
        void 
        parse_cmd();
        
        PicoOsUart u{ PicoOsUart(0 , UART0_TX_PIN, UART0_RX_PIN, 115200) };
        EEPROM* e;
        std::array<uint8_t, 64> buf;
        uint8_t* ptr{ buf.data() };
};

#endif
