#include "shell.hpp"
#include <cstring>
#include <charconv>

#define INACTIVE -1

static inline bool 
stop(char c)
{
        return c == '\r'  || c == '\n';
}

Shell::Shell(EEPROM* e)
: e(e)
{
}

void 
Shell::loop() 
{
        // Initial read, wake the task up
        ptr = buf.data();
        u.read(ptr, 1, portMAX_DELAY);
        if(stop(*ptr)) {
                // Empty command line
                return;
        }
        u.write(ptr++, 1, 0);

        // Read subsequent bytes
        for(int res; res = read_chars();) {
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
        constexpr const char* msg{ "Usage: [help || time || interval <time>]\r\n"
                "Options:\r\n"
                "       help:   - print this help\r\n"
                "       time:   - print last timestamp of toggle\r\n"
                "   interval:   - change led interval to specified time\r\n" };
        u.send(msg);
}

int
Shell::read_chars()
{
        int bytes{ u.read(ptr, 1, pdMS_TO_TICKS(30000)) };
        if(bytes == 0) {
                // Inactive >30s
                constexpr const char* msg{ "\r\n[Inactive]\r\n" };
                u.send(msg);
                return INACTIVE;
        }

        // Parse char
        switch(*ptr) {
        default:
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
                constexpr const char* end{ "\r\n" };
                u.write((uint8_t*)end, 2, 0);
                return 0;
        }
}

void
Shell::parse_value(std::string_view& cmd)
{
}

void 
Shell::parse_cmd()
{
        //Sanitize command
        std::string_view cmd{ (char*)buf.data() };
        auto pos{ cmd.find_first_of("\r\n") };
        cmd = cmd.substr(0, pos);

        //Parse command
        if(cmd == "help") {
                print_usage();
        } else {
                constexpr const char* msg{ "Unknown command, try help\r\n" };
                u.send(msg);
        }
}
