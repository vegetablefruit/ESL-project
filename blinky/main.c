#include <nrfx_pwm.h>
#include <nrfx_gpiote.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <nrfx_systick.h>
#include <app_timer.h>
#include <app_error.h>
#include <math.h>

#define BUTTON NRF_GPIO_PIN_MAP(1, 6)
#define LED_1 NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(0, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)
#define LED_ACTIVE_LOW 1

#define DEBOUNCE_MS 15
#define DOUBLE_CLICK_US (400 * 1000u)
#define HOLD_BUTTON_MS 60

static uint32_t hue = 356;
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

static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t pwm_vals = {0, 0, 0, 0};
static nrf_pwm_sequence_t pwm_seq =
    {
        .values.p_individual = &pwm_vals,
        .length = 1,
        .repeats = 0,
        .end_delay = 0};

static nrfx_systick_state_t last_click_time;
static nrfx_systick_state_t hold_time;
static bool waiting_second_click = false;

static nrfx_systick_state_t indicator_last_toggle;
static bool indicator = false;

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

static void debounce_timer_handler(void *p_context)
{
    bool stable = (nrf_gpio_pin_read(BUTTON) == 0);

    if (stable != button_stable_pressed)
    {
        button_stable_pressed = stable;

        if (stable)
        {
            nrfx_systick_get(&hold_time);

            if (waiting_second_click)
            {
                nrfx_systick_state_t now;
                nrfx_systick_get(&now);

                if (!nrfx_systick_test(&last_click_time, DOUBLE_CLICK_US) == false)
                {
                    mode = (input_mode_t)((mode + 1) % 4);
                    waiting_second_click = false;
                }
            }
            else
            {
                waiting_second_click = true;
                nrfx_systick_get(&last_click_time);
            }
        }
    }
}

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    app_timer_start(btn_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL);
}

void gpiote_init()
{
    nrfx_gpiote_init();

    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    nrfx_gpiote_in_init(BUTTON, &config, button_handler);
    nrfx_gpiote_in_event_enable(BUTTON, true);
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

int main(void)
{
    nrfx_systick_init();
    app_timer_init();                                                                          // ★
    app_timer_create(&btn_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timer_handler); // ★

    gpiote_init();
    pwm_init();

    update_color_from_hsv();

    nrfx_systick_get(&indicator_last_toggle);
    nrfx_systick_get(&hold_time);

    while (1)
    {
        if (button_stable_pressed)
        {
            if (nrfx_systick_test(&hold_time, HOLD_BUTTON_MS * 1000U))
            {
                nrfx_systick_get(&hold_time);

                if (mode == MODE_HUE)
                    hue = (hue + 1) % 360;
                else if (mode == MODE_SAT && saturation < 100)
                    saturation++;
                else if (mode == MODE_VAL && value < 100)
                    value++;

                update_color_from_hsv();
            }
        }

        indicator_update();
        sleep_cpu();
    }
}
