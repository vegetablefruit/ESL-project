#include <stdbool.h>
#include <stdint.h>

#include "nordic_common.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_log_backend_usb.h"

#include "app_usbd.h"
#include "app_usbd_serial_num.h"

#include <nrfx_pwm.h>
#include <nrfx_systick.h>
#include <app_timer.h>
#include <nrf_gpio.h>
#include <nrfx_gpiote.h>
#include "nrfx_clock.h"

#define BUTTON NRF_GPIO_PIN_MAP(1, 6)
#define LED_1 NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(0, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)
#define LED_ACTIVE_LOW 1

#define DEBOUNCE_MS 50

typedef enum
{
    BUTTON_OFF,
    BUTTON_ON
} button_states_t;

typedef enum
{
    MODE_NONE = 0,
    MODE_HUE,
    MODE_SAT,
    MODE_VAL
} input_mode_t;

APP_TIMER_DEF(btn_debounce_timer);

static button_states_t button_state = BUTTON_OFF;

// static uint32_t hue = 356; // 360 * 0.99
// static uint32_t saturation = 100;
// static uint32_t value = 100;
#if 0
static int input_mode = MODE_NONE;
static nrfx_systick_state_t indicator_last_toggle;
static bool indicator = false;
#endif

static void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void debounce_timer_handler(void *p_context);

static inline void sleep_cpu(void);
static inline void led_on_gpio(uint32_t pin);
static inline void led_off_gpio(uint32_t pin);

#if 0
static void switch_hsv_input_mode();
static void indicator_update(void);
#endif

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals = {0, 0, 0, 0};
static nrf_pwm_sequence_t pwm_seq =
    {
        .values.p_individual = &pwm_vals,
        .length = 1,
        .repeats = 0,
        .end_delay = 0};

void pwm_init(void)
{
    nrfx_pwm_config_t config =
        {
            .output_pins = {LED_R, LED_G, LED_B, NRFX_PWM_PIN_NOT_USED},
            .irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY,
            .base_clock = NRF_PWM_CLK_1MHz,
            .count_mode = NRF_PWM_MODE_UP,
            .top_value = 1000,
            .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
            .step_mode = NRF_PWM_STEP_AUTO};

    nrfx_pwm_init(&pwm0, &config, NULL);
    nrfx_pwm_simple_playback(&pwm0, &pwm_seq, 1, NRFX_PWM_FLAG_LOOP);
}

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
}

int main(void)
{
    logs_init();

    NRF_LOG_INFO("Starting up the test project with USB logging");

    pwm_init();
    gpiote_init();
    clock_init();
    timer_init();

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();
#if 0
        indicator_update();
        sleep_cpu();
#endif
    }
}

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    app_timer_stop(btn_debounce_timer);
    app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
}

static void debounce_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_ON;
        led_on_gpio(LED_1);
    }
    else
    {
        button_state = BUTTON_OFF;
        led_off_gpio(LED_1);
    }
}

#if 0
static void switch_hsv_input_mode()
{
    input_mode = (input_mode + 1) % 4;
    NRF_LOG_INFO("HSV input mode %d", input_mode);
}

static void indicator_update(void)
{
    nrfx_systick_state_t now;
    nrfx_systick_get(&now);

    switch (input_mode)
    {
    case MODE_NONE:
        led_off_gpio(LED_1);
        break;

    case MODE_HUE:
        if (nrfx_systick_test(&indicator_last_toggle, 500000))
        {
            indicator_last_toggle = now;
            indicator = !indicator;
            if (indicator)
                led_on_gpio(LED_1);
            else
                led_off_gpio(LED_1);
        }
        break;

    case MODE_SAT:
        if (nrfx_systick_test(&indicator_last_toggle, 120000))
        {
            indicator_last_toggle = now;
            indicator = !indicator;
            if (indicator)
                led_on_gpio(LED_1);
            else
                led_off_gpio(LED_1);
        }
        break;

    case MODE_VAL:
        led_on_gpio(LED_1);
        break;
    }
}
#endif

static inline void led_off_gpio(uint32_t pin)
{
#if LED_ACTIVE_LOW
    nrf_gpio_pin_set(pin);
#else
    nrf_gpio_pin_clear(pin);
#endif
}

static inline void led_on_gpio(uint32_t pin)
{
#if LED_ACTIVE_LOW
    nrf_gpio_pin_clear(pin);
#else
    nrf_gpio_pin_set(pin);
#endif
}

static inline void sleep_cpu(void)
{
    __WFE();
    __SEV();
    __WFI();
}