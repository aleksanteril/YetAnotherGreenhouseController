#include "shell.hpp"
#include <cstring>
#include <charconv>

static inline bool 
stop(char c)
{
        return c == '\r'  || c == '\n';
}

void
Shell::inactivity_timer(TimerHandle_t xTimer)
{
        // Shell->semaph
        auto semaph{ (SemaphoreHandle_t)pvTimerGetTimerID(xTimer) };
        xSemaphoreGive(semaph);
}

Shell::Shell()
{
        if (!semaph) {
                printf("SHELL: Semaphore could not be created\n");
                for (;;);
        }
        vQueueAddToRegistry(semaph, "INACTIVE FLAG");
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
        xTimerStart(inactivity_tmr, 0);
        // Read subsequent bytes, semaph from inactivity
        while (read_cmd() == false) {
                if (xSemaphoreTake(semaph, 0)) {
                        // Inactivity flag
                        constexpr const char* msg{ "\n[Inactive]\n" };
                        u.send(msg);
                        return;
                }
        }

        // Succesfull command, newline
        xTimerStop(inactivity_tmr, 0);
        parse_cmd();
}

void
Shell::print_usage()
{
        constexpr const char* msg{ "Usage: [help || time || interval <time>]\n"
                "Options:\n"
                "       help:   - print this help\n"
                "       time:   - print last timestamp of toggle\n"
                "   interval:   - change led interval to specified time\n" };
        u.send(msg);
}

bool 
Shell::read_cmd()
{
        int bytes{ u.read(ptr, 1, pdMS_TO_TICKS(10)) };
        if(bytes == 0) {
                // Nothing written
                return false;
        }
        xTimerReset(inactivity_tmr, 0);
        
        // Parse char
        switch(*ptr) {
        default:
                if (ptr < buf.end()) {
                        // Writing pointer in limits
                        u.write(ptr++, 1, 0);
                }
                return false;
        case '\x7f': // DEL
        case '\b':
                if (ptr-1 >= buf.data()) {
                        // Clear, backspace - clear - backspace
                        constexpr const char* clear{ "\b \b" };
                        u.write((uint8_t*)clear, 3, 0);
                        --ptr;
                }
                return false;
        case '\r':
        case '\n':
                constexpr char nl{ '\n' };
                u.write((uint8_t*)&nl, 1, 0);
                return true;
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
                constexpr const char* msg{ "Unknown command, try help\n" };
                u.send(msg);
        }
}
