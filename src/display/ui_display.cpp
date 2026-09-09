#include "metropolia_io.hpp"
#include "ui_display.hpp"
#include "hardware/gpio.h"
#include "task.h"
#include <string>
#include <span>

QueueHandle_t DisplayEvent_q{ nullptr };

static void
button_task(void* param) 
{
        const uint pin{ (uint)param };
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        bool pressed{};
        for(;;) {
                vTaskDelay(pdMS_TO_TICKS(220));
                if(gpio_get(pin)) {
                        pressed = false;
                } else if(!pressed) {
                        pressed = true;
                        Event e{ PRESS };
                        xQueueSend(DisplayEvent_q, (void*)&e, 0);
                }
        }
}

static void 
callback_irq(uint gpio, uint32_t event_mask)
{
        auto xTaskWoken{ pdFALSE };
        Event e = gpio_get(ROT_B_PIN) ? RIGHT : LEFT;
        xQueueSendFromISR(DisplayEvent_q, (void*)&e, &xTaskWoken);
        portYIELD_FROM_ISR(xTaskWoken);
}


UI_Display::UI_Display(EEPROM* e)
  : eeprom(e)
{
        gpio_init(ROT_A_PIN);
        gpio_set_dir(ROT_A_PIN, GPIO_IN);
        gpio_init(ROT_B_PIN);
        gpio_set_dir(ROT_B_PIN, GPIO_IN);
        DisplayEvent_q = event_q;
        gpio_set_irq_enabled_with_callback(
                ROT_A_PIN, GPIO_IRQ_EDGE_FALL, true, callback_irq);
        xTaskCreate(button_task, "BUTTON", 64, 
                (void*)ROT_SW_PIN, tskIDLE_PRIORITY + 2, NULL);
}

void
UI_Display::inactivity_timer(TimerHandle_t xTimer)
{
        Event e{ INACTIVE };
        xQueueSend(DisplayEvent_q, &e, 0);
}

void
UI_Display::change_state_to(State st)
{
        (this->*state)(Exit);
        state = st;
        (this->*state)(Enter);
}

// Helper function for info state
static int
read_info_eeprom(EEPROM* e, std::span<char> ipv4, std::span<char> setp)
{
        uint32_t ipaddr{};
        uint16_t setpoint{};
        bool ip_r{ e->load_ipaddr(&ipaddr) };
        bool set_r{ e->load_setpoint(&setpoint) };
        if(!ip_r || !set_r) {
                return -1;
        }
        // Convert IPv4 to decimal notation
        uint8_t a, b, c, d;
        inet_to_dec(ipaddr, &a, &b, &c, &d);
        snprintf(setp.data(), setp.size(), "- %u ppm", setpoint);
        snprintf(ipv4.data(), setp.size(), "- %u.%u.%u.%u", a,b,c,d);
        return 2;
}

void
UI_Display::info(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                display.fill(0);
                if (read_info_eeprom(eeprom, info_ipv4, info_setp) < 0) {
                        display.text("Failed to load!", 0, 0, 1);
                } else {
                        display.text("CO2 Target", 0, 0, 1);
                        display.text(info_setp.data(), 8, 10, 1);
                        display.text("Cloud IPv4", 0, 24, 1);
                        display.text(info_ipv4.data(), 8, 37, 1);
                }
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case PRESS:
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }   
}

void
UI_Display::set_ipv4(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                display.fill(0);
                display.text("ipv4", 0, 0, 1);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }   
}

void
UI_Display::set_target(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                display.fill(0);
                display.text("target", 0, 0, 1);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }   
}

// Helper function for menu state
static void
handle_menu_index(Event ev, int& i)
{
        // Menu item count is hardcoded here, ihan sama
        (ev == RIGHT) ? (++i) : (--i);
        if(i <= 0) {
                i = 0;
        } else if (i > 2) {
                i = 2;
        }
}

void
UI_Display::menu(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                menu_i = 0;
                display.fill(0);
                display.text("Info", 18, 0, 1);
                display.text("Set CO2", 18, 9, 1);
                display.text("Set IPv4", 18, 18, 1);
                display.text("->", 0, menu_i*9, 1);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case RIGHT:
                case LEFT:
                        xTimerReset(inact_t, 0);
                        handle_menu_index(ev, menu_i);
                        display.rect(0, 0, 18, 64, 0, true);
                        display.text("->", 0, menu_i*9, 1);
                        display.show();
                        break;
                case PRESS:
                        change_state_to(items.at(menu_i));
                        break;
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }
}

void
UI_Display::status(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                display.fill(0);
                display.text("Status", 0, 0, 1);
                display.show();
                break;
        case Loop:
                switch(ev) {
                default:
                        change_state_to(&UI_Display::menu);
                        break;
                case INACTIVE:
                        break;
                }
        }
}

void
UI_Display::loop()
{
        display.fill(0);
        display.text("Boot", 0, 0, 1);
        display.show();
        for(;;) {
                xQueueReceive(DisplayEvent_q, (void*)&ev, portMAX_DELAY);
                (this->*state)(Loop);
        }
}
