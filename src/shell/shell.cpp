#include "shell.hpp"
#include <cstring>
#include <charconv>
#include <optional>
#include "constants.hpp"

#include "hardware/watchdog.h"

constexpr int INACTIVE{ -1 };

bool
unallowed_ascii(uint8_t* ptr) {
        uint8_t ch{ *ptr };
        bool res{};
        if(ch >= 128 || ch <= 255) {
                // allow nordics etc, extended ascii
                return false;
        }
        return (ch < 32 || ch > 127);
}

static inline std::optional<int>
parse_setpoint(std::string_view& cmd)
{
        int setpoint{};
        char extra{};
        if(sscanf(cmd.data(), "%d %c", &setpoint, &extra) != 1) {
                return std::nullopt;
        }
        if(setpoint > MAX_CO2_SP || setpoint < 0) {
                return std::nullopt;
        }
        return setpoint;
}

static inline std::optional<Cache::NetworkSetting_t>
parse_wlan(std::string_view& cmd)
{
        Cache::NetworkSetting_t packet;
        char extra{};
        if(sscanf(cmd.data(), "\"%32[^\"]\" \"%63[^\"]\" %c", 
           &packet.ssid, &packet.pwd, &extra) != 2) {
                return std::nullopt;
        }
        return packet;
}

constexpr uint UART0_TX_PIN{ 0 };
constexpr uint UART0_RX_PIN{ 1 };
constexpr int UART0_BAUD{ 115200 };
Shell::Shell(EEPROM& e)
: u(0, UART0_TX_PIN, UART0_RX_PIN, UART0_BAUD), e(e)
{
}

void 
Shell::loop() 
{
        // Initial read, wake the task up
        ptr = buf.data();
        u.read(ptr, 1, portMAX_DELAY);
        bool first_ch{ true };

        // Read subsequent bytes
        for(int res; res = read_chars(first_ch);) {
                if(res ==  INACTIVE) {
                        return;
                }
        }
        // Succesfull command, newline
        parse_cmd();
}

void
Shell::print_usage()
{
        constexpr const char* msg{
                "Options:\r\n"
                "             help:   - print this help\r\n"
                "             info:   - print current saved settings\r\n"
                "     target <ppm>:   - set co2 ppm level (0 - 1500)\r\n" 
                "wlan \"ssid\" \"pwd\":   - set wlan connection settings\r\n" };
        u.send(msg);
}

void
Shell::print_settings()
{
        uint16_t setpoint{};
        if(!e.load_setpoint(setpoint)) {
                constexpr const char* fail{ 
                        "Failed to load settings\r\n" };
                u.send(fail);
                return;
        }
        // Convert IPv4 to decimal notation
        std::array<char, 64> msg;
        snprintf(msg.data(), msg.size(), 
                "- CO2 Setpoint: %u ppm\r\n", setpoint);
        u.send(msg.data());
}

int
Shell::read_chars(bool& skip_read)
{
        if(!skip_read) {
                int bytes{ u.read(ptr, 1, pdMS_TO_TICKS(30000)) };
                if(bytes == 0) {
                        // Inactive >30s
                        constexpr const char* msg{ "\r\n[Inactive]\r\n" };
                        u.send(msg);
                        return INACTIVE;
                }
        } else {
                skip_read = false;
        }

        // Parse char
        switch(*ptr) {
        default:
                if(unallowed_ascii(ptr)) {
                        // destroy potential ansi sequences other garbage etc
                        uint8_t dummy;
                        while(u.read(&dummy, 1, 0));
                        return 1;
                }
                if(ptr < buf.end()-1) {
                        // Writing pointer in limits end() - 1
                        // The last place in buffer is dummy char for read()
                        u.write(ptr++, 1, 0);
                }
                return 1;
        case '\x7f': // DEL
        case '\b':
                if(ptr-1 >= buf.data()) {
                        // Clear, backspace - clear - backspace
                        constexpr const char* clear{ "\b \b" };
                        u.write((uint8_t*)clear, 3, 0);
                        --ptr;
                }
                return 1;
        case '\r':
        case '\n':
                {
                        // Incase of \r\n read the subsequent LF
                        char dummy{};
                        u.read((uint8_t*)&dummy, 1, pdMS_TO_TICKS(5));
                }
                constexpr const char* end{ "\r\n" };
                u.write((uint8_t*)end, 2, 0);
                return 0;
        }
}

void 
Shell::parse_cmd()
{
        //Sanitize command
        std::string_view cmd{ (char*)buf.data(), (size_t)(ptr - buf.data()) };
        if(cmd.empty()) {
                // Empty command
                return;
        }

        // hack, make this compatible with c string funcs
        // scanf is much easier to use with patterns
        *ptr = '\0';

        constexpr const char* fail_parse{ "Invalid value\r\n" };
        constexpr const char* fail_save{ "Local value set.\r\n"
                                         "EEPROM Save failed.\r\n" };
        //Parse command
        if(cmd == "help" || cmd == "?") {
                print_usage();
        } else if(cmd == "info") {
                print_settings();
        } else if(cmd.starts_with("target ")) {
                cmd.remove_prefix(sizeof("target ")-1);
                auto setpoint{ parse_setpoint(cmd) };
                if(!setpoint) {
                        u.send(fail_parse);
                        return;
                }
                if (!e.save_setpoint(*setpoint)) {
                        u.send(fail_save);
                }
        } else if(cmd.starts_with("wlan ")) {
                cmd.remove_prefix(sizeof("wlan ")-1);
                auto wlan{ parse_wlan(cmd) };
                if(!wlan) {
                        u.send(fail_parse);
                        return;    
                }
                if (!e.save_network(*wlan)) {
                        u.send(fail_save);
                } else {
                        // reset device to enable new network
                        // trying to change IPStack on the fly causes
                        // tcp-pcb panic errors :(
                        watchdog_reboot(0, 0, 1000);
                }
        } else {
                constexpr const char* msg{ "Unknown command, try help\r\n" };
                u.send(msg);
        }
}
