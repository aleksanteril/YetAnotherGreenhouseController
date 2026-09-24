#include "ui_display.hpp"
#include "hardware/gpio.h"
#include "task.h"
#include <string>
#include <span>
#include <optional>
#include "constants.hpp"
#include <sensors.hpp>

using namespace Display;

QueueHandle_t Display::Event_q{ nullptr };

static EventGroupHandle_t intGo{ nullptr };
constexpr int PROCEED{ 0x1 };

constexpr uint ROT_A_PIN{ 10 }; //ext pull-up
constexpr uint ROT_B_PIN{ 11 }; //ext pull-up
constexpr uint ROT_SW_PIN{ 12 };

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
                        xQueueSend(Event_q, (void*)&pin, 0);
                }
                xEventGroupWaitBits(intGo, PROCEED, pdFALSE, pdTRUE, portMAX_DELAY);
        }
}

static void 
callback_irq(uint gpio, uint32_t event_mask)
{
        auto xTaskWoken{ pdFALSE };
        Event e = gpio_get(ROT_B_PIN) ? RIGHT : LEFT;
        xQueueSendFromISR(Event_q, (void*)&e, &xTaskWoken);
        portYIELD_FROM_ISR(xTaskWoken);
}

UI_Display::UI_Display(EEPROM& e)
  : eeprom(e)
{
        gpio_init(ROT_A_PIN);
        gpio_set_dir(ROT_A_PIN, GPIO_IN);
        gpio_init(ROT_B_PIN);
        gpio_set_dir(ROT_B_PIN, GPIO_IN);
        // for global events to display (sensor updates etc)
        Event_q = e_q;
        vQueueAddToRegistry(Event_q, "DisplayEvent_q");
        // try not to pollute namespace, for button task internal use
        intGo = sw_tasks;
        gpio_set_irq_enabled_with_callback(
                ROT_A_PIN, GPIO_IRQ_EDGE_FALL, true, callback_irq);
        xEventGroupSetBits(sw_tasks, PROCEED);
        xTaskCreate(button_task, "BUTTON_SW0", 64, 
                (void*)SW0_PIN, tskIDLE_PRIORITY + 2, nullptr);
        xTaskCreate(button_task, "BUTTON_SW2", 64, 
                (void*)SW2_PIN, tskIDLE_PRIORITY + 2, nullptr);
        change_state_to(&UI_Display::menu);
}

void
UI_Display::inactivity_timer(TimerHandle_t xTimer)
{
        Event e{ INACTIVE };
        xQueueSend(Event_q, &e, 0);
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
                        display.text("CO2 TARGET", 24, 0, 1);
                        display.text(info_setp.data(), 16, 10, 1);
                } else {
                        display.text("EEPROM Fail!", 0, 0, 1);
                        display.text("Failed to load!", 0, 9, 1);
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
handle_draw_val(ssd1306os& display, std::span<int> set_val, int menu_i)
{
        std::array<char, 32> buf;
        display.rect(0, 24, 128, 12, 0, true);
        snprintf(buf.data(), buf.size(), "%1u %1u %1u %1u", 
                set_val[0],set_val[1],set_val[2],set_val[3]);
        display.text(buf.data(), 36, 24, 1);
        display.text("_", 36+(menu_i*16), 28, 1);
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
        if(setpoint < 0 || setpoint > MAX_CO2_SP) {
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
                display.text("SET CO2 TARGET", 9, 0, 1);
                display.text("SW2 TO SAVE", 18, 54, 1);
                handle_draw_val(display, set_val, menu_i);
                display.show();
                xTimerReset(inact_t, 0);
                break;
        case Loop:
                switch(ev) {
                case RIGHT:
                case LEFT:
                        xTimerReset(inact_t, 0);
                        handle_var(ev, set_val[menu_i], 0, 9);
                        handle_draw_val(display, set_val, menu_i);
                        display.show();
                        break;
                case PRESS_SW0:
                        xTimerReset(inact_t, 0);
                        menu_i = ++menu_i % set_val.size();
                        handle_draw_val(display, set_val, menu_i);
                        display.show();
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
                                display.text("Local value set.", 0, 0, 1);
                                display.text("EEPROM Save fail!", 0, 9, 1);
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
                display.text("INFO", 18, 0, 1);
                display.text("SET CO2 TARGET", 18, 9, 1);
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
handle_draw_sensors(ssd1306os& display)
{
        Sensors::SensorPacket_t packet;
        xQueuePeek(Sensors::co2_mb, &packet, 0);
        uint16_t co2{ packet.u16 };
        xQueuePeek(Sensors::rh_mb, &packet, 0);
        double rh{ packet.f64 };
        xQueuePeek(Sensors::temp_mb, &packet, 0);
        double temp{ packet.f64 };
        uint16_t sp{};
        xQueuePeek(Cache::setpoint_mb, &sp, 0);

        display.fill(0);
        std::array<char, 32> buf;
        snprintf(buf.data(), buf.size(), "[TEMP: %6.1lf C]", temp);
        display.text(buf.data(), 0, 0, 1);
        snprintf(buf.data(), buf.size(), "[RH: %8.1lf %%]", rh);
        display.text(buf.data(), 0, 12, 1);
        snprintf(buf.data(), buf.size(), "[CO2: %5u ppm]", co2);
        display.text(buf.data(), 0, 24, 1);
        snprintf(buf.data(), buf.size(), "%4u - SP", sp);
        display.text(buf.data(), 56, 36, 1);
        display.show();
}

void
UI_Display::status(enum intEvent _e)
{
        switch(_e) {
        case Enter:
                // Button tasks sleep
                xEventGroupClearBits(sw_tasks, PROCEED);
                handle_draw_sensors(display);
                break;
        case Loop:
                if(ev == UPDATE) {
                        handle_draw_sensors(display);
                } else if(ev != INACTIVE) {
                        change_state_to(&UI_Display::menu);
                }
        case Exit:
                // button tasks continue
                xEventGroupSetBits(sw_tasks, PROCEED);
        }
}

void
UI_Display::loop()
{
        for(;;) {
                xQueueReceive(Event_q, (void*)&ev, portMAX_DELAY);
                (this->*state)(Loop);
        }
}
