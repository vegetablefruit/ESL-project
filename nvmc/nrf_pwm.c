#include "nrf_pwm.h"
#include "nrf_hsv.h"
#include <nrfx_pwm.h>
#include <nrf_log.h>


static uint16_t rgb_r = 0;
static uint16_t rgb_g = 0;
static uint16_t rgb_b = 0;


static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);

static nrf_pwm_values_individual_t pwm_vals = {0, 0, 1000, 0};

static nrf_pwm_sequence_t pwm_seq =
    {
        .values.p_individual = &pwm_vals,
        .length = 4,
        .repeats = 0,
        .end_delay = 0};

void pwm_init(void)
{
    nrfx_pwm_config_t pwm0_config =
        {
            .output_pins = {LED_R, LED_G, LED_B, NRFX_PWM_PIN_NOT_USED},
            .irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY,
            .base_clock = NRF_PWM_CLK_1MHz,
            .count_mode = NRF_PWM_MODE_UP,
            .top_value = 1000,
            .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
            .step_mode = NRF_PWM_STEP_AUTO};

    nrfx_pwm_init(&pwm0, &pwm0_config, NULL);
    nrfx_pwm_simple_playback(&pwm0, &pwm_seq, 1, NRFX_PWM_FLAG_LOOP);

    hsv_to_rgb(hue, sat, val);
    pwm_update_from_rgb();                                                                                                          
}

void pwm_update_from_rgb(void)
{
    const uint16_t top = 1000;

    pwm_vals.channel_0 = (rgb_r * top) / 255;
    pwm_vals.channel_1 = (rgb_g * top) / 255;
    pwm_vals.channel_2 = (rgb_b * top) / 255;

    NRF_LOG_INFO("new pwm values %d %d %d", pwm_vals.channel_0, pwm_vals.channel_1, pwm_vals.channel_2);

    nrfx_pwm_sequence_update(&pwm0, 0, &pwm_seq);
}
