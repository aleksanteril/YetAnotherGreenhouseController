#ifndef EEPROM_HPP
#define EEPROM_HPP

#include "FreeRTOS.h"
#include "crc16.h"
#include "PicoI2C.h"
#include <cstring>
#include "event_groups.h"

#include "secret.h"

namespace Cache
{
        constexpr uint16_t DEFAULT_SETPOINT{ 1000 };
        extern QueueHandle_t setpoint_mb;

        struct NetworkSetting_t {
                uint16_t crc;
                char ssid[33];
                char pwd[64];
                TickType_t timestamp;
        };
        constexpr NetworkSetting_t DEFAULT_NETWORK {
                .crc = 0,
                .ssid = WIFI_SSID,
                .pwd = WIFI_PASSWORD,
                .timestamp = 0
        };
        extern QueueHandle_t network_mb;
}

/* EEPROM 0x50 is the only device used on i2c0 */
class EEPROM : private PicoI2C {
        public:
                EEPROM(uint bus_nr=0);
                EEPROM(const EEPROM&) = delete;
                bool 
                save_setpoint(uint16_t setpoint);
                bool
                load_setpoint(uint16_t& setpoint);
                bool
                save_network(Cache::NetworkSetting_t& packet);
                bool
                load_network(Cache::NetworkSetting_t& packet);

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
                void
                xor_encrypt(uint8_t* bytes, size_t len, uint8_t key);
};

#endif
