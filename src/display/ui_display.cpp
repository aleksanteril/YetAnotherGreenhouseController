#include "metropolia_io.hpp"
#include "ui_display.hpp"
#include "hardware/gpio.h"
#include "task.h"
#include <string>

static QueueHandle_t e_q;

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
                        xQueueSend(e_q, (void*)&e, 0);
                }
        }
}

static void 
callback_irq(uint gpio, uint32_t event_mask)
{
        auto xTaskWoken{ pdFALSE };
        Event e = gpio_get(ROT_B_PIN) ? RIGHT : LEFT;
        xQueueSendFromISR(e_q, (void*)&e, &xTaskWoken);
        portYIELD_FROM_ISR(xTaskWoken);
}


UI_Display::UI_Display(EEPROM* e)
  : e(e)
{
        gpio_init(ROT_A_PIN);
        gpio_set_dir(ROT_A_PIN, GPIO_IN);
        gpio_init(ROT_B_PIN);
        gpio_set_dir(ROT_B_PIN, GPIO_IN);
        // To limit scope
        e_q = event_q;
        gpio_set_irq_enabled_with_callback(
                ROT_A_PIN, GPIO_IRQ_EDGE_FALL, true, callback_irq);
        xTaskCreate(button_task, "BUTTON", 64, 
                (void*)ROT_SW_PIN, tskIDLE_PRIORITY + 2, NULL);
}

void
UI_Display::inactivity_timer(TimerHandle_t xTimer)
{
        Event e{ INACTIVE };
        xQueueSend(e_q, &e, 0);
}

void
UI_Display::loop()
{
        int num{};
        Event e;
        display.fill(0);
        display.text("Boot", 0, 0, 1);
        display.show();
        for(;;) {
                xQueueReceive(e_q, (void*)&e, portMAX_DELAY);
                display.fill(0);
                switch(state) {
                case MENU:
                        if(e == INACTIVE) {
                                state = STATUS;
                                display.text("STATUS READ", 0, 0, 1);
                                break;
                        }
                        xTimerReset(inact_t, 0);
                        display.text("MENU", 0, 0, 1);
                        break;
                case STATUS:
                        if(e != INACTIVE) {
                                state = MENU;
                                display.text("MENU", 0, 0, 1);
                                break;
                        }
                        break;
                }
                display.show();
        }
}
