#ifndef SHELL_HPP
#define SHELL_HPP

#include "FreeRTOS.h"
#include <array>
#include "PicoOsUart.h"
#include "eeprom.hpp"

class Shell {
      public:
        explicit Shell(EEPROM&);
        Shell(const Shell&) = delete;
        void 
        loop();

      private:
        void 
        print_usage();
        void
        print_settings();
        int 
        read_chars(bool&);
        void 
        parse_cmd();
        
        PicoOsUart u;
        EEPROM& e;
        std::array<uint8_t, 64> buf;
        uint8_t* ptr{ buf.data() };
};

#endif
