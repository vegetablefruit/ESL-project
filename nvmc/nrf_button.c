#include "nrf_button.h"

#include <stdbool.h>
#include <app_timer.h>
#include <nrf_gpio.h>
#include <nrfx_gpiote.h>
#include <nrf_log.h>
#include <nrfx_systick.h>

#include "board.h"

APP_TIMER_DEF(btn_debounce_timer);
APP_TIMER_DEF(btn_double_click_timer);
APP_TIMER_DEF(btn_long_click_timer);
APP_TIMER_DEF(mode_blink_timer);

uint32_t hue = 356;
uint32_t sat = 255;
uint32_t val = 255;

static int input_mode = MODE_NONE;

extern void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v);
extern void pwm_update_from_rgb(void);

static button_states_t button_state = BUTTON_OFF;
static bool wait_first_click = true;
static bool led1_state = false;

void led_on_gpio(uint32_t pin)
{
#if LED_ACTIVE_LOW
    nrf_gpio_pin_clear(pin);
#else
    nrf_gpio_pin_set(pin);
#endif
}

void led_off_gpio(uint32_t pin)
{
#if LED_ACTIVE_LOW
    nrf_gpio_pin_set(pin);
#else
    nrf_gpio_pin_clear(pin);
#endif
}

static void switch_hsv_input_mode(void)
{
    input_mode = (input_mode + 1) % 4;
    NRF_LOG_INFO("HSV input mode %d", input_mode);

    app_timer_stop(mode_blink_timer);

    switch (input_mode)
    {
    case MODE_NONE:
        led_off_gpio(LED_1);
        break;

    case MODE_HUE:
        led_on_gpio(LED_1);
        app_timer_start(mode_blink_timer,
                        APP_TIMER_TICKS(1000), NULL);
        break;

    case MODE_SAT:
        led_on_gpio(LED_1);
        app_timer_start(mode_blink_timer,
                        APP_TIMER_TICKS(200), NULL);
        break;

    case MODE_VAL:
        led_on_gpio(LED_1);
        break;
    }
}

void button_timers_init()
{
    app_timer_create(&btn_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timer_handler);
    app_timer_create(&btn_double_click_timer, APP_TIMER_MODE_SINGLE_SHOT, double_click_timer_handler);
    app_timer_create(&btn_long_click_timer, APP_TIMER_MODE_SINGLE_SHOT, long_click_timer_handler);
    app_timer_create(&mode_blink_timer, APP_TIMER_MODE_REPEATED, mode_button_handler);
}

void button_init(void)
{
    nrfx_gpiote_init();

    nrf_gpio_cfg_output(LED_1);
    led_off_gpio(LED_1);

    nrfx_gpiote_in_config_t cfg =
        NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    cfg.pull = NRF_GPIO_PIN_PULLUP;

    nrfx_gpiote_in_init(BUTTON, &cfg, button_handler);
    nrfx_gpiote_in_event_enable(BUTTON, true);

    button_timers_init();
}

void button_handler(nrfx_gpiote_pin_t pin,
                    nrf_gpiote_polarity_t action)
{
    app_timer_stop(btn_debounce_timer);
    app_timer_start(btn_debounce_timer,
                    APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
}

void debounce_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_ON;
        led_on_gpio(LED_1);

        app_timer_start(btn_long_click_timer,
                        APP_TIMER_TICKS(LONG_CLICK_MS), NULL);
    }
    else
    {
        app_timer_stop(btn_long_click_timer);

        if (button_state == BUTTON_ON)
        {
            if (wait_first_click)
            {
                wait_first_click = false;
                app_timer_start(btn_double_click_timer,
                                APP_TIMER_TICKS(DOUBLE_CLICK_MS),
                                NULL);
            }
            else
            {
                wait_first_click = true;
                switch_hsv_input_mode();
            }
        }
        button_state = BUTTON_OFF;
    }
}

void long_click_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_LONG;
        NRF_LOG_INFO("long click");
    }
}

void double_click_timer_handler(void *p_context)
{
    if (!wait_first_click)
    {
        wait_first_click = true;
        NRF_LOG_INFO("single click");
    }
}

void mode_button_handler(void *p_context)
{
    led1_state = !led1_state;

    if (led1_state)
        led_on_gpio(LED_1);
    else
        led_off_gpio(LED_1);
}

void main_timer_handler(void *p_context)
{
    if (button_state != BUTTON_LONG)
        return;

    switch (input_mode)
    {
    case MODE_HUE:
        hue = (hue + 3) % 360;
        break;

    case MODE_SAT:
        sat = (sat < 250) ? sat + 5 : 0;
        break;

    case MODE_VAL:
        val = (val < 250) ? val + 5 : 0;
        break;

    default:
        return;
    }

    hsv_to_rgb(hue, sat, val);
    pwm_update_from_rgb();
}
