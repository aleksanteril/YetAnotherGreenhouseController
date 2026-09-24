#include "eeprom.hpp"

QueueHandle_t Cache::setpoint_mb{ nullptr };
QueueHandle_t Cache::network_mb{ nullptr };

// Memory alignment, 
// SETPOINT 2B CRC16 2B ; NETWORK_CRC16 2B ; SSID 33B
// PWD 64B

constexpr uint SETPOINT_ADDR{ 0x0 };
constexpr uint SSID_ADDR{ SETPOINT_ADDR+(sizeof(uint16_t)*2) };
constexpr uint PWD_ADDR{ 0x40 };

constexpr int GO_BITS{ 0x1 };
constexpr int EEPROM_ADDR{ 0x50 };
constexpr int I2C_SPEED{ 400000 };

/* EEPROM 0x50 is the only device used on i2c0 */
EEPROM::EEPROM(uint bus_nr)
: PicoI2C(bus_nr, I2C_SPEED), dev_addr(EEPROM_ADDR)
{
        Cache::setpoint_mb = xQueueCreate(1, sizeof(uint16_t));
        Cache::network_mb = xQueueCreate(1, 
                                sizeof(Cache::NetworkSetting_t));
        vQueueAddToRegistry(Cache::setpoint_mb, "SETPOINT_MB");
        vQueueAddToRegistry(Cache::network_mb, "NETWORK_MB");
        uint16_t sp = Cache::DEFAULT_SETPOINT;
        xQueueOverwrite(Cache::setpoint_mb, (void*)&sp);
        auto packet = Cache::DEFAULT_NETWORK;
        xQueueOverwrite(Cache::network_mb, &packet);
        xEventGroupSetBits(go, GO_BITS);
}

bool 
EEPROM::save_setpoint(uint16_t setpoint)
{
        // Hook to change setpoint even though it doesnt save, allowing
        // use without EEPROM
        xQueueOverwrite(Cache::setpoint_mb, (void*)&setpoint);
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
EEPROM::load_setpoint(uint16_t& setpoint)
{
        uint8_t buf[4];
        if (!read_page(SETPOINT_ADDR, buf, 4)) {
                return false;
        }
        if (crc16(buf, 4) != 0) {
                return false;
        }
        setpoint = ((uint16_t)buf[0] << 8U) | buf[1];
        xQueueOverwrite(Cache::setpoint_mb, &setpoint);
        return true;
}

bool
EEPROM::save_network(Cache::NetworkSetting_t& packet)
{
        //  crc16 + ssid + pwd
        // saved on 2 sections
        uint8_t buf[2+33+64];
        memcpy(buf+2, packet.ssid, 33);
        memcpy(buf+2+33, packet.pwd, 64);
        packet.crc = crc16(buf+2, 33+64);
        xQueueOverwrite(Cache::network_mb, (void*)&packet);
        buf[0] = (uint8_t)(packet.crc >> 8U);
        buf[1] = (uint8_t)packet.crc;
        xor_encrypt(buf, 2+33+64, KEY);
        if(!write_page(SSID_ADDR, buf, 2+33)) {
                return false;
        }
        return write_page(PWD_ADDR, buf+2+33, 64);
}

bool
EEPROM::load_network(Cache::NetworkSetting_t& packet)
{
        uint8_t buf[2+33+64+2];
        if(!read_page(SSID_ADDR, buf, 2+33)) {
                return false;
        }
        if(!read_page(PWD_ADDR, buf+2+33, 64)) {
                return false;
        }
        xor_encrypt(buf, 2+33+64, KEY);
        // crc16 function requires the crc appended so this dirty crap will do
        buf[2+33+64] = buf[0];
        buf[2+33+64+1] = buf[1];
        if(crc16(buf+2, 33+64+2) != 0) {
                return false;
        }
        packet.crc = ((uint16_t)buf[0] << 8U) | buf[1];
        memcpy(packet.ssid, buf+2, 33);
        memcpy(packet.pwd, buf+2+33, 64);
        xQueueOverwrite(Cache::network_mb, (void*)&packet);
        return true;
}

void
EEPROM::xor_encrypt(uint8_t* bytes, size_t len, uint8_t key)
{
        for(int i{}; i < len; ++i, ++bytes) {
                *bytes ^= key;
        }
}

// True on success
bool 
EEPROM::read_page(uint16_t addr, uint8_t* bytes, size_t len)
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
EEPROM::write_page(uint16_t addr, uint8_t* bytes, size_t len)
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
