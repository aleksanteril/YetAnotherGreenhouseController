#ifndef EEPROM_HPP
#define EEPROM_HPP

#include "crc16.h"
#include "PicoI2C.h"
#include <cstring>
#include "intnet.h"
#include "event_groups.h"

// Memory alignment, SETPOINT 2b CRC16 2b ; IPADDR 4b CRC16 2b
#define SETPOINT_ADDR 0x0
#define NETWORK_ADDR 0x64

constexpr int GO_BITS{ 0x1 };

/* EEPROM 0x50 is the only device used on i2c0 */
class EEPROM : private PicoI2C {
        public:
                EEPROM(uint8_t addr=0x50, uint bus_nr=0)
                : PicoI2C(bus_nr, 400000), dev_addr(addr)
                {
                        xEventGroupSetBits(go, GO_BITS);
                }
                EEPROM(const EEPROM&) = delete;

                bool 
                save_setpoint(uint16_t setpoint)
                {
                        // 4 bytes
                        uint8_t buf[4];
                        buf[0] = (uint8_t)(setpoint >> 8U);
                        buf[1] = (uint8_t)setpoint;
                        auto crc{ crc16(buf, 2) };
                        buf[2] = (uint8_t)(crc >> 8U);
                        buf[3] = (uint8_t)crc;
                        return write_page(SETPOINT_ADDR, buf, 4);
                }

                bool
                load_setpoint(uint16_t& setpoint)
                {
                        uint8_t buf[4];
                        if (read_page(SETPOINT_ADDR, buf, 4) == false) {
                                return false;
                        }
                        if (crc16(buf, 4) != 0) {
                                return false;
                        }
                        setpoint = ((uint16_t)buf[0] << 8U) | buf[1];
                        return true;
                }

        private:
                const uint8_t dev_addr;
                EventGroupHandle_t go{ xEventGroupCreate() };

                // True on success
                bool 
                read_page(uint16_t addr, uint8_t* bytes, size_t len)
                {
                        uint8_t frame[2];
                        frame[0] = (uint8_t)(addr >> 8U);
                        frame[1] = (uint8_t)addr;
                        // go read if no-one is writing or after 10ms write
                        xEventGroupWaitBits(go, GO_BITS, 
                                pdFALSE, pdTRUE, portMAX_DELAY);
                        return transaction(dev_addr, 
                                frame, 2, bytes, len) == len+2;
                }

                // NOTE! len must be 64 or under due to EEPROM page size!
                // True on success
                bool 
                write_page(uint16_t addr, uint8_t* bytes, size_t len)
                {
                        if(len > 64) {
                                return false;
                        }
                        uint8_t frame[2+64];
                        frame[0] = (uint8_t)(addr >> 8U);
                        frame[1] = (uint8_t)addr;
                        memcpy(frame+2, bytes, len);
                        // go write, clear go bit
                        xEventGroupWaitBits(go, GO_BITS, pdTRUE, pdTRUE, portMAX_DELAY);
                        bool result{ write(dev_addr, frame, len+2) == len+2 };
                        if (result) {
                                // data coherency force tasks on event flag to wait
                                vTaskDelay(pdMS_TO_TICKS(10));                                                                
                        }
                        // set go bit ok again
                        xEventGroupSetBits(go, GO_BITS);
                        return result;
                }
};

#endif
