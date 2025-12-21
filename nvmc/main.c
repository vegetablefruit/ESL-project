#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "nrf_button.h"
#include "nrf_pwm.h"
#include "nrf_hsv.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_log_backend_usb.h"

#include <app_timer.h>
#include <nrfx_clock.h>
#include <nrf_gpio.h>
#include <nrfx_gpiote.h>
#include <nrfx_systick.h>

APP_TIMER_DEF(btn_debounce_timer);
APP_TIMER_DEF(btn_double_click_timer);
APP_TIMER_DEF(btn_long_click_timer);
APP_TIMER_DEF(main_timer);
APP_TIMER_DEF(mode_blink_timer);

//static button_states_t button_state = BUTTON_OFF;
//static bool wait_first_click = true;
//static bool led1_state = false;
//static int input_mode = MODE_NONE;

//static inline void led_on_gpio(uint32_t pin);
//static inline void led_off_gpio(uint32_t pin);

//static void switch_hsv_input_mode();

void clock_init()
{
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running())
        ;
}

void gpiote_init()
{
    nrfx_gpiote_init();

    nrf_gpio_cfg_output(LED_1);
    led_off_gpio(LED_1);

    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    nrfx_gpiote_in_init(BUTTON, &config, button_handler);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}

void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void timer_init()
{
    app_timer_init();
    app_timer_create(&btn_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timer_handler);
    app_timer_create(&btn_double_click_timer, APP_TIMER_MODE_SINGLE_SHOT, double_click_timer_handler);
    app_timer_create(&btn_long_click_timer, APP_TIMER_MODE_SINGLE_SHOT, long_click_timer_handler);
    app_timer_create(&main_timer, APP_TIMER_MODE_REPEATED, main_timer_handler);
    app_timer_create(&mode_blink_timer, APP_TIMER_MODE_REPEATED, mode_button_handler);
}

int main(void)
{
    logs_init();

    NRF_LOG_INFO("Starting up the test project with USB logging");

    pwm_init();
    gpiote_init();
    clock_init();
    timer_init();

    app_timer_start(main_timer, APP_TIMER_TICKS(100), NULL);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();
    }
}