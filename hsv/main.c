#include <stdbool.h>
#include <stdint.h>

#include "nordic_common.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_log_backend_usb.h"

#include "app_usbd.h"
#include "app_usbd_serial_num.h"

#include <nrfx_systick.h>
#include <app_timer.h>
#include <nrfx_gpiote.h>
#include <nrf_gpio.h>
#include "nrfx_clock.h"

#define BUTTON NRF_GPIO_PIN_MAP(1, 6)

#define DEBOUNCE_MS 50
#define HOLD_BUTTON_MS 500
#define SHORT_CLICK_MS 50

typedef enum
{
    BUTTON_OFF,
    BUTTON_DEBOUNCE,
    BUTTON_LONG_CLICK
} button_states_t;

APP_TIMER_DEF(btn_debounce_timer);
APP_TIMER_DEF(btn_short_click_timer);
APP_TIMER_DEF(btn_long_click_timer);

// static nrfx_systick_state_t last_click_time;

static button_states_t button_state = BUTTON_OFF;
static bool debounced = false;

static void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void debounce_timer_handler(void *p_context);
static void btn_short_click_timer_handler(void *p_context);
static void btn_long_click_timer_handler(void *p_context);
static void handle_long_click(void);

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
    app_timer_create(&btn_short_click_timer, APP_TIMER_MODE_SINGLE_SHOT, btn_short_click_timer_handler);
    app_timer_create(&btn_long_click_timer, APP_TIMER_MODE_SINGLE_SHOT, btn_long_click_timer_handler);
}

int main(void)
{
    logs_init();

    NRF_LOG_INFO("Starting up the test project with USB logging");

    gpiote_init();
    clock_init();
    timer_init();

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();
    }
}

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    switch (button_state)
    {
    case BUTTON_OFF:
        NRF_LOG_INFO("state off");
        if (nrf_gpio_pin_read(BUTTON) == 0)
        {
            NRF_LOG_INFO("to debounce");
            button_state = BUTTON_DEBOUNCE;
            debounced = false;
            app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
        }
        else
        {
            NRF_LOG_INFO("to off!!!");
            button_state = BUTTON_OFF;
        }
        break;
    case BUTTON_DEBOUNCE:
        NRF_LOG_INFO("state debounce");
        if (debounced)
        {
            NRF_LOG_INFO("debounced");
            if (nrf_gpio_pin_read(BUTTON) == 0)
            {
                NRF_LOG_INFO("to long click");
                button_state = BUTTON_LONG_CLICK;
                // TODO handle long click
                app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(HOLD_BUTTON_MS), NULL);
            }
            else
            {
                NRF_LOG_INFO("to off");
                button_state = BUTTON_OFF;
                // TODO handle short click
                app_timer_start(btn_short_click_timer, APP_TIMER_TICKS(SHORT_CLICK_MS), NULL);
            }
        }
        break;
    case BUTTON_LONG_CLICK:
        // NRF_LOG_INFO("state long click");
        if (nrf_gpio_pin_read(BUTTON) == 0)
        {
            NRF_LOG_INFO("to debounce");
            button_state = BUTTON_DEBOUNCE;
            // TODO start debounce timer
            app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
        }
        else
        {
            NRF_LOG_INFO("to off");
            button_state = BUTTON_OFF;
            // TODO handle long click finished
            // app_timer_start(btn_short_click_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
        }
        break;

    default:
        NRF_LOG_INFO("state unknown");
        if (nrf_gpio_pin_read(BUTTON) == 0)
        {
            NRF_LOG_INFO("to long click");
            button_state = BUTTON_LONG_CLICK;
        }
        else
        {
            NRF_LOG_INFO("to off");
            button_state = BUTTON_OFF;
        }
    }
}

static void debounce_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        debounced = true;
        NRF_LOG_INFO("timer");
        // TODO start btn_short_click_timer
        button_state = BUTTON_LONG_CLICK;
        app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(HOLD_BUTTON_MS), NULL);
    }
    else
    {
        button_state = BUTTON_OFF;
    }
}

static void btn_short_click_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_LONG_CLICK;
        // Start handling long click
        NRF_LOG_INFO("long click detected");
        app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(HOLD_BUTTON_MS), NULL);
    }
    else
    {
        button_state = BUTTON_OFF;
        NRF_LOG_INFO("short click detected");
        app_timer_start(btn_short_click_timer, APP_TIMER_TICKS(SHORT_CLICK_MS), NULL);
    }
}

static void btn_long_click_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_LONG_CLICK;
        // Start handling long click
        NRF_LOG_INFO("long click detected!");
        // switch mode
        handle_long_click();
    }
    else
    {
        NRF_LOG_INFO("short click detected!!!");
        button_state = BUTTON_OFF;
    }
}

static void handle_long_click(void)
{
    NRF_LOG_INFO("long click handled");
}