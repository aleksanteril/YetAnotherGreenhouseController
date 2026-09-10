#include "metropolia_io.hpp"
#include "ui_display.hpp"
#include "hardware/gpio.h"
#include "task.h"
#include <string>
#include <span>
#include <string>
#include "intnet.h"
#include <optional>

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
                vTaskDelay(pdMS_TO_TICKS(200));
                if(gpio_get(pin)) {
                        pressed = false;
                } else if(!pressed) {
                        pressed = true;
                        xQueueSend(DisplayEvent_q, (void*)&pin, 0);
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


UI_Display::UI_Display(EEPROM& e)
  : eeprom(e)
{
        gpio_init(ROT_A_PIN);
        gpio_set_dir(ROT_A_PIN, GPIO_IN);
        gpio_init(ROT_B_PIN);
        gpio_set_dir(ROT_B_PIN, GPIO_IN);
        DisplayEvent_q = event_q;
        gpio_set_irq_enabled_with_callback(
                ROT_A_PIN, GPIO_IRQ_EDGE_FALL, true, callback_irq);
        xTaskCreate(button_task, "BUTTON_SW0", 64, 
                (void*)SW0_PIN, tskIDLE_PRIORITY + 2, NULL);
        xTaskCreate(button_task, "BUTTON_SW2", 64, 
                (void*)SW2_PIN, tskIDLE_PRIORITY + 2, NULL);
        change_state_to(&UI_Display::menu);
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
static bool
read_info_eeprom(EEPROM& e, std::span<char> setp)
{
        uint16_t setpoint{};
        if(!e.load_setpoint(setpoint)) {
                return false;
        }
        snprintf(setp.data(), setp.size(), "- %4u ppm -", setpoint);
        return true;
}

void
UI_Display::info(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                display.fill(0);
                if (read_info_eeprom(eeprom, info_setp)) {
                        display.text("CO2 Target", 24, 0, 1);
                        display.text(info_setp.data(), 16, 10, 1);
                } else {
                        display.text("Failed to load!", 0, 0, 1);
                }
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case PRESS_SW0:
                        // timer is reset on enter to menu
                        change_state_to(&UI_Display::menu);
                        break;
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }   
}

void
UI_Display::wait(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case PRESS_SW0:
                        // timer is reset on enter to menu
                        change_state_to(&UI_Display::menu);
                        break;
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
        }
}

// small helper function to handle var between min and max
static void
handle_var(Event ev, int& val, int min, int max)
{
        (ev == RIGHT) ? (++val) : (--val);
        val = (val < min) ? (max) : (val % (max+1));
}

// helper function for similar ipv4 and setpoint setting
void
handle_draw_val(ssd1306os& display, std::span<int> set_val)
{
        char buf[32];
        display.rect(0, 24, 128, 8, 0, true);
        snprintf(buf, 32, "%1u %1u %1u %1u", 
                set_val[0],set_val[1],set_val[2],set_val[3]);
        display.text(buf, 36, 24, 1);
}

static std::optional<bool>
save_valid_setpoint(EEPROM& e, std::span<int> vals)
{
        //fast way to write but uses std::string heap etc
        std::string temp;
        for(auto val : vals) {
                temp += std::to_string(val);
        }
        uint16_t setpoint = std::stoi(temp);
        if(setpoint < 0 || setpoint > 1500) {
                return std::nullopt;
        }
        return e.save_setpoint(setpoint);
}

void
UI_Display::set_target(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                menu_i = 0; // Repurpose this variable
                display.fill(0);
                set_val.fill(0);
                display.text("Set CO2 target", 9, 0, 1);
                handle_draw_val(display, set_val);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case RIGHT:
                case LEFT:
                        xTimerReset(inact_t, 0);
                        handle_var(ev, set_val[menu_i], 0, 9);
                        handle_draw_val(display, set_val);
                        display.show();
                        break;
                case PRESS_SW0:
                        xTimerReset(inact_t, 0);
                        menu_i = ++menu_i % set_val.size();
                        break;
                case PRESS_SW2:
                        xTimerReset(inact_t, 0);
                        if (std::optional<bool> res; 
                                !(res = save_valid_setpoint(eeprom, set_val))) {
                                display.fill(0);
                                display.text("Invalid value!", 0, 0, 1);
                                display.show();
                                change_state_to(&UI_Display::wait);
                        } else if (!*res) {
                                //save failed
                                display.fill(0);
                                display.text("Save failed!", 0, 0, 1);
                                display.show();
                                change_state_to(&UI_Display::wait);
                        } else {
                                //save ok
                                change_state_to(&UI_Display::menu);
                        }
                        break;
                case INACTIVE:
                        change_state_to(&UI_Display::status);
                        break;
                }
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
                display.text("->", 0, menu_i*9, 1);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case RIGHT:
                case LEFT:
                        xTimerReset(inact_t, 0);
                        handle_var(ev, menu_i, 0, items.size()-1);
                        display.rect(0, 0, 18, 64, 0, true);
                        display.text("->", 0, menu_i*9, 1);
                        display.show();
                        break;
                case PRESS_SW0:
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
        for(;;) {
                xQueueReceive(DisplayEvent_q, (void*)&ev, portMAX_DELAY);
                (this->*state)(Loop);
        }
}
