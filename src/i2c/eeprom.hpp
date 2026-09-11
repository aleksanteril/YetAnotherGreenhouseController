#ifndef EEPROM_HPP
#define EEPROM_HPP

#include "FreeRTOS.h"
#include "crc16.h"
#include "PicoI2C.h"
#include <cstring>
#include "event_groups.h"

/* EEPROM 0x50 is the only device used on i2c0 */
class EEPROM : private PicoI2C {
        public:
                EEPROM(uint bus_nr=0);
                EEPROM(const EEPROM&) = delete;
                bool 
                save_setpoint(uint16_t setpoint);
                bool
                load_setpoint(uint16_t& setpoint);

        private:
                const uint8_t dev_addr;
                const EventGroupHandle_t go{ xEventGroupCreate() };
                // True on success
                bool 
                read_page(uint16_t addr, uint8_t* bytes, size_t len);
                // NOTE! len must be 64 or under due to EEPROM page size!
                // True on success
                bool 
                write_page(uint16_t addr, uint8_t* bytes, size_t len);
};

#endif
