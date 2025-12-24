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

APP_TIMER_DEF(main_timer);

// static button_states_t button_state = BUTTON_OFF;
// static bool wait_first_click = true;
// static bool led1_state = false;
// static int input_mode = MODE_NONE;

// static inline void led_on_gpio(uint32_t pin);
// static inline void led_off_gpio(uint32_t pin);

// static void switch_hsv_input_mode();

void clock_init()
{
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running())
        ;
}

void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

int main(void)
{
    logs_init();
    app_timer_init();

    NRF_LOG_INFO("Starting up the test project with USB logging");

    pwm_init();
    button_init();
    clock_init();

    app_timer_create(&main_timer, APP_TIMER_MODE_REPEATED, main_timer_handler);
    app_timer_start(main_timer, APP_TIMER_TICKS(100), NULL);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();
    }
}