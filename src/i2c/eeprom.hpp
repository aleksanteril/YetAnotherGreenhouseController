#ifndef EEPROM_HPP
#define EEPROM_HPP

#include "crc16.h"
#include "PicoI2C.h"
#include <cstring>

// Memory alignment, SETPOINT 2b CRC16 2b ; IPADDR 4b CRC16 2b
#define SETPOINT_ADDR 0x0
#define IPADDR_ADDR SETPOINT_ADDR+4

/* EEPROM 0x50 is the only device used on i2c0 */
class EEPROM : private PicoI2C {
        public:
                EEPROM(uint8_t addr=0x50, uint bus_nr=0)
                : PicoI2C(bus_nr, 400000), dev_addr(addr)
                {
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
                load_setpoint(uint16_t* setpoint)
                {
                        uint8_t buf[4];
                        if (read_page(SETPOINT_ADDR, buf, 4) == false) {
                                return false;
                        }
                        if (crc16(buf, 4) != 0) {
                                return false;
                        }
                        *setpoint = ((uint16_t)buf[0] << 8U) | buf[1];
                        return true;
                }

                bool
                save_ipaddr(uint32_t ipaddr)
                {
                        uint8_t buf[6];
                        buf[0] = (uint8_t)(ipaddr >> 24U);
                        buf[1] = (uint8_t)(ipaddr >> 16U);
                        buf[2] = (uint8_t)(ipaddr >> 8U);
                        buf[3] = (uint8_t)ipaddr;
                        auto crc{ crc16(buf, 4) };
                        buf[4] = (uint8_t)(crc >> 8U);
                        buf[5] = (uint8_t)crc;
                        return write_page(IPADDR_ADDR, buf, 6);
                }

                bool
                load_ipaddr(uint32_t* ipaddr)
                {
                        uint8_t buf[6];
                        if (read_page(IPADDR_ADDR, buf, 6) == false) {
                                return false;
                        }
                        if (crc16(buf, 6) != 0) {
                                return false;
                        }
                        *ipaddr = ((uint32_t)buf[0] << 24U) | 
                                  ((uint32_t)buf[1] << 16U) |
                                  ((uint32_t)buf[2] << 8U) | buf[3];
                        return true;
                }


        private:
                const uint8_t dev_addr;

                // True on success
                bool
                read_byte(uint16_t addr, uint8_t* byte)
                {
                        uint8_t frame[2];
                        frame[0] = (uint8_t)(addr >> 8U);
                        frame[1] = (uint8_t)addr;
                        return transaction(dev_addr, frame, 2, byte, 1) == 3;
                }

                // True on success
                bool 
                write_byte(uint16_t addr, uint8_t byte)
                {
                        uint8_t frame[3];
                        frame[0] = (uint8_t)(addr >> 8U);
                        frame[1] = (uint8_t)addr;
                        frame[2] = byte;
                        return write(dev_addr, frame, 3) == 3;
                }

                // True on success
                bool 
                read_page(uint16_t addr, uint8_t* bytes, size_t len)
                {
                        uint8_t frame[2];
                        frame[0] = (uint8_t)(addr >> 8U);
                        frame[1] = (uint8_t)addr;
                        return transaction(dev_addr, frame, 2, bytes, len) == len+2;
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
                        return write(dev_addr, frame, len+2) == len+2;
                }
};

#endif
