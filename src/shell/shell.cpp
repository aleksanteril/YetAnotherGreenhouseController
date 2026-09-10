#include "shell.hpp"
#include <cstring>
#include <charconv>
#include <optional>

#define INACTIVE -1

static inline std::optional<uint32_t>
parse_ip(std::string_view& cmd)
{
        uint octets[4];
        if(sscanf(cmd.data(), "%u.%u.%u.%u", 
                octets, octets+1, octets+2, octets+3) != 4) {
                return std::nullopt;
        }
        for(auto octet : octets) {
                if(octet > 255) {
                        // invalid
                        return std::nullopt;
                }
        }
        uint32_t ipaddr{ 
                dec_to_inet(octets[0], octets[1], octets[2], octets[3]) };
        if(!valid_ip(ipaddr)) {
                return std::nullopt;
        }
        return ipaddr;
}

static inline std::optional<int>
parse_setpoint(std::string_view& cmd)
{
        int setpoint{};
        if(sscanf(cmd.data(), "%d", &setpoint) != 1) {
                return std::nullopt;
        }
        if(setpoint > 1500 || setpoint < 0) {
                return std::nullopt;
        }
        return setpoint;
}

Shell::Shell(EEPROM& e)
: e(e)
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
                "                help:   - print this help\r\n"
                "                info:   - print current saved settings\r\n"
                "    set target <ppm>:   - set co2 ppm level (0 - 1500)\r\n" };
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
                if(*ptr < 32 || *ptr > 127) {
                        // destroy potential ansi sequences other garbage etc
                        uint8_t dummy;
                        while(u.read(&dummy, 1, 0));
                        return 1;
                }
                if (ptr < buf.end()-1) {
                        // Writing pointer in limits end() - 1
                        // The last place in buffer is dummy char for read()
                        u.write(ptr++, 1, 0);
                }
                return 1;
        case '\x7f': // DEL
        case '\b':
                if (ptr-1 >= buf.data()) {
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
        constexpr const char* fail_save{ "Save failed, try again\r\n" };
        //Parse command
        if(cmd == "help") {
                print_usage();
        } else if (cmd == "info") {
                print_settings();
        } else if (cmd.starts_with("set target ")) {
                cmd.remove_prefix(sizeof("set target ")-1);
                auto setpoint{ parse_setpoint(cmd) };
                if(!setpoint) {
                        u.send(fail_parse);
                        return;
                } else if (e.save_setpoint(*setpoint) == false) {
                        u.send(fail_save);
                }
        } else {
                constexpr const char* msg{ "Unknown command, try help\r\n" };
                u.send(msg);
        }
}
