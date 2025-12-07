#include <nrfx_systick.h>
#include <app_timer.h>
#include <nrfx_gpiote.h>
#include <nrf_gpio.h>
#include "nrfx_clock.h"
#include <nrf_delay.h>
#include <nrfx_pwm.h>

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// #include "nordic_common.h"

// include "nrf_log.h"
// #include "nrf_log_ctrl.h"
// #include "nrf_log_default_backends.h"

// #include "nrf_log_backend_usb.h"

// #include "nrf_usbd.h"
//  #include "app_usbd_serial_num.h"

#define BUTTON NRF_GPIO_PIN_MAP(1, 6)
#define LED_1 NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(0, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)
#define LED_ACTIVE_LOW 1

#define DEBOUNCE_MS 50
#define HOLD_BUTTON_MS 500
#define SHORT_CLICK_MS 50
#define DOUBLE_CLICK_MS 40

static void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void debounce_timer_handler(void *p_context);
static void btn_short_click_timer_handler(void *p_context);
static void btn_long_click_timer_handler(void *p_context);
static void color_change_timer_handler(void *p_context);
static void handle_long_click(void);
static void handle_short_click(void);

static inline void led_on_gpio(uint32_t pin);
static inline void led_off_gpio(uint32_t pin);
static void hsv_to_rgb(uint32_t H_deg, uint32_t S_per, uint32_t V_per,
                       float *out_r, float *out_g, float *out_b);
static void apply_rgb_to_pwm(float r, float g, float b);
static void update_color_from_hsv(void);
static void indicator_update(void);
static inline void sleep_cpu(void);

typedef enum
{
    BUTTON_OFF,
    BUTTON_DEBOUNCE,
    BUTTON_LONG_CLICK
} button_states_t;

static uint32_t hue = 356; // 360 * 0.99
static uint32_t saturation = 100;
static uint32_t value = 100;

typedef enum
{
    MODE_NONE = 0,
    MODE_HUE,
    MODE_SAT,
    MODE_VAL
} input_mode_t;

static volatile input_mode_t mode = MODE_NONE;
static volatile bool button_stable_pressed = false;
static volatile bool raw_state = false;

APP_TIMER_DEF(btn_debounce_timer);
APP_TIMER_DEF(btn_short_click_timer);
APP_TIMER_DEF(btn_long_click_timer);
APP_TIMER_DEF(color_change_timer);

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals = {0, 0, 0, 0};
static nrf_pwm_sequence_t pwm_seq =
    {
        .values.p_individual = &pwm_vals,
        .length = 1,
        .repeats = 0,
        .end_delay = 0};

static nrfx_systick_state_t first_click_time;
static nrfx_systick_state_t last_click_time;
// static nrfx_systick_state_t hold_time;
static bool waiting_second_click = false;

static button_states_t button_state = BUTTON_OFF;
static bool debounced = false;

static nrfx_systick_state_t indicator_last_toggle;
static bool indicator = false;

void clock_init()
{
    nrfx_clock_init(NULL);
    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running())
        ;
}

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

void gpiote_init()
{
    nrfx_gpiote_init();

    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    nrfx_gpiote_in_init(BUTTON, &config, button_handler);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}

/*
void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}
*/
void timer_init()
{
    app_timer_init();
    app_timer_create(&btn_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timer_handler);
    app_timer_create(&btn_short_click_timer, APP_TIMER_MODE_SINGLE_SHOT, btn_short_click_timer_handler);
    app_timer_create(&btn_long_click_timer, APP_TIMER_MODE_SINGLE_SHOT, btn_long_click_timer_handler);
    app_timer_create(&color_change_timer, APP_TIMER_MODE_SINGLE_SHOT, color_change_timer_handler);
}

int main(void)
{
    // logs_init();
    nrfx_systick_init();
    pwm_init();

    nrf_gpio_cfg_output(LED_1);
    led_off_gpio(LED_1);

    nrfx_systick_get(&indicator_last_toggle);

    // NRF_LOG_INFO("Starting up the test project with USB logging");

    gpiote_init();
    clock_init();
    timer_init();

    __enable_irq();

    for (int i = 0; i < 10; i++)
    {
        nrf_gpio_pin_toggle(LED_1);
        nrf_delay_ms(200);
    }

    while (1)
    {
        indicator_update();
        sleep_cpu();
    }
}

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    switch (button_state)
    {
    case BUTTON_OFF:
        // NRF_LOG_INFO("state off");
        if (nrf_gpio_pin_read(BUTTON) == 0)
        {
            // NRF_LOG_INFO("to debounce");
            button_state = BUTTON_DEBOUNCE;
            debounced = false;
            app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
        }
        else
        {
            // NRF_LOG_INFO("to off!!!");
            button_state = BUTTON_OFF;
        }
        break;
    case BUTTON_DEBOUNCE:
        // NRF_LOG_INFO("state debounce");
        if (debounced)
        {
            // NRF_LOG_INFO("debounced");
            if (nrf_gpio_pin_read(BUTTON) == 0)
            {
                // NRF_LOG_INFO("to long click");
                button_state = BUTTON_LONG_CLICK;
                // TODO handle long click
                app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(HOLD_BUTTON_MS), NULL);
            }
            else
            {
                // NRF_LOG_INFO("to off");
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
            // NRF_LOG_INFO("to debounce");
            button_state = BUTTON_DEBOUNCE;
            // TODO start debounce timer
            app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
            app_timer_start(color_change_timer, APP_TIMER_TICKS(30), NULL);
        }
        else
        {
            // NRF_LOG_INFO("to off");
            button_state = BUTTON_OFF;
            // TODO handle long click finished
            app_timer_stop(btn_long_click_timer);
        }
        break;

    default:
        // NRF_LOG_INFO("state unknown");
        if (nrf_gpio_pin_read(BUTTON) == 0)
        {
            // NRF_LOG_INFO("to long click");
            button_state = BUTTON_LONG_CLICK;
        }
        else
        {
            // NRF_LOG_INFO("to off");
            button_state = BUTTON_OFF;
        }
    }
}

static void debounce_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        // debounced = true;
        //  NRF_LOG_INFO("timer");
        //   TODO start btn_short_click_timer
        button_state = BUTTON_LONG_CLICK;
        app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(HOLD_BUTTON_MS), NULL);
    }
    else
    {
        waiting_second_click = true;
        nrfx_systick_get(&last_click_time);
        button_state = BUTTON_OFF;
    }
}

static void btn_short_click_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
        return; // Если кнопка ещё нажата → ждём long click

    // Проверим двойной клик
    nrfx_systick_state_t now;
    nrfx_systick_get(&now);

    if (waiting_second_click && !nrfx_systick_test(&first_click_time, DOUBLE_CLICK_MS))
    {
        waiting_second_click = false;
        // ДВОЙНОЙ КЛИК → смена режима
        mode = (input_mode_t)((mode + 1) % 4);
        indicator = false;
        return;
    }

    // первый клик
    waiting_second_click = true;
    nrfx_systick_get(&first_click_time);
}

static void btn_long_click_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        button_state = BUTTON_LONG_CLICK;
        // Start handling long click
        // NRF_LOG_INFO("long click detected!");
        // switch mode
        handle_long_click();
        app_timer_start(btn_long_click_timer, APP_TIMER_TICKS(30), NULL);
    }
    else
    {
        // NRF_LOG_INFO("short click detected!!!");
        button_state = BUTTON_OFF;
        handle_short_click();
    }
}

static void handle_long_click(void)
{
    // NRF_LOG_INFO("long click handled");

    switch (mode)
    {
    case MODE_HUE:
        hue++; // медленное изменение
        if (hue > 359)
            hue = 0;
        break;

    case MODE_SAT:
        if (saturation < 100)
            saturation++;
        break;

    case MODE_VAL:
        if (value < 100)
            value++;
        break;

    default:
        break;
    }

    update_color_from_hsv();
}

static void handle_short_click(void)
{
    // NRF_LOG_INFO("short click handled");
    nrfx_systick_state_t now;
    nrfx_systick_get(&now);

    if (waiting_second_click)
    {
        // Проверяем: второй клик успел вовремя?
        if (!nrfx_systick_test(&first_click_time, DOUBLE_CLICK_MS))
        {
            // NRF_LOG_INFO("DOUBLE CLICK!!! Switching mode");

            waiting_second_click = false;

            // менять режим
            mode = (input_mode_t)((mode + 1) % 4);
            indicator = false;

            return;
        }

        // слишком поздно → считаем как новый первый клик
        waiting_second_click = false;
    }

    // это ПЕРВЫЙ short click → начинаем ждать второй
    waiting_second_click = true;
    nrfx_systick_get(&first_click_time);

    // NRF_LOG_INFO("Single click (waiting second)");
}

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

static void hsv_to_rgb(uint32_t H_deg, uint32_t S_per, uint32_t V_per,
                       float *out_r, float *out_g, float *out_b)
{
    float H = (float)H_deg;
    float S = (float)S_per / 100.0f;
    float V = (float)V_per / 100.0f;

    float C = V * S;
    float Hp = H / 60.0f;
    float X = C * (1.0f - fabsf(fmodf(Hp, 2.0f) - 1.0f));
    float m = V - C;

    float r = 0, g = 0, b = 0;

    if (Hp < 1)
    {
        r = C;
        g = X;
    }
    else if (Hp < 2)
    {
        r = X;
        g = C;
    }
    else if (Hp < 3)
    {
        g = C;
        b = X;
    }
    else if (Hp < 4)
    {
        g = X;
        b = C;
    }
    else if (Hp < 5)
    {
        r = X;
        b = C;
    }
    else
    {
        r = C;
        b = X;
    }

    *out_r = r + m;
    *out_g = g + m;
    *out_b = b + m;
}

static void apply_rgb_to_pwm(float r, float g, float b)
{
    uint16_t top = 1000;

#if LED_ACTIVE_LOW
    pwm_vals.channel_0 = top - (uint16_t)(r * top);
    pwm_vals.channel_1 = top - (uint16_t)(g * top);
    pwm_vals.channel_2 = top - (uint16_t)(b * top);
#else
    pwm_vals.channel_0 = (uint16_t)(r * top);
    pwm_vals.channel_1 = (uint16_t)(g * top);
    pwm_vals.channel_2 = (uint16_t)(b * top);
#endif
}

static void update_color_from_hsv(void)
{
    float r, g, b;
    hsv_to_rgb(hue, saturation, value, &r, &g, &b);
    apply_rgb_to_pwm(r, g, b);
}

static void color_change_timer_handler(void *p_context)
{
    if (nrf_gpio_pin_read(BUTTON) == 0)
    {
        switch (mode)
        {
        case MODE_HUE:
            hue = (hue + 1) % 360;
            break;
        case MODE_SAT:
            if (saturation < 100)
                saturation++;
            break;
        case MODE_VAL:
            if (value < 100)
                value++;
            break;
        case MODE_NONE:
        default:

            break;
        }
        update_color_from_hsv();
    }
}

static void indicator_update(void)
{
    nrfx_systick_state_t now;
    nrfx_systick_get(&now);

    switch (mode)
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

static inline void sleep_cpu(void)
{
    __WFE();
    __SEV();
    __WFI();
}