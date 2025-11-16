#include <nrfx_pwm.h>
#include <nrfx_gpiote.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <nrfx_systick.h>

// 7199
#define BUTTON NRF_GPIO_PIN_MAP(1, 6)
#define LED_1 NRF_GPIO_PIN_MAP(0, 6)
#define LED_R NRF_GPIO_PIN_MAP(0, 8)
#define LED_G NRF_GPIO_PIN_MAP(0, 9)
#define LED_B NRF_GPIO_PIN_MAP(0, 12)

// sequence
uint32_t led_seq[] = {
    LED_1, LED_1, LED_1, LED_1, LED_1, LED_1, LED_1,               // 7times
    LED_R,                                                         // 1 times
    LED_G, LED_G, LED_G, LED_G, LED_G, LED_G, LED_G, LED_G, LED_G, // 9 times
    LED_B, LED_B, LED_B, LED_B, LED_B, LED_B, LED_B, LED_B, LED_B  // 9 times
};
#define SEQ_LENGTH (sizeof(led_seq) / sizeof(led_seq[0]))

static uint32_t led_index = 0;
static nrfx_pwm_t pwm0 = NRFX_PWM_INSTANCE(0);

static uint16_t pwm_value = 0;  
static nrf_pwm_sequence_t pwm_seq = {
    .values.p_common = &pwm_value,
    .length = 1,
    .repetition = 0,
    .end_delay = 0
}

void init_pwm(uint32_t pin){
    nrfx_pwm_config_t config = {
        .output_pins = {
            pin,
            NRFX_PWM_PIN_NOT_USED,
            NRFX_PWM_PIN_NOT_USED,
            NRFX_PWM_PIN_NOT_USED
        },
        .irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY,
        .base_clock = NRF_PWM_CLK_1MHz,
        .count_mode = NRF_PWM_MODE_UP,
        .top_value = 1000,
        .load_mode = NRF_PWM_LOAD_COMMON,
        .step_mode = NRF_PWM_STEP_AUTO
    }
    nrfx_pwm_init(&pwm0, &config, NULL);
    nrfx_pwm_simple_playback(&pwm0, &pwm_seq, 1, NRFX_PWM_FLAG_LOOP);
}

//duty cycle
void pwm_set_duty_cycle(uint16_t duty){
    if (duty > 1000){
        duty = 1000;
        pwm_value = duty;
    }
}

void pwm_switch_led(uint32_t pin){
    nrfx_pwm_uninit(&pwm0);
    pwm_init(pin);
}

//GPIOTE
volatile uint32_t last_click_time = 0;
volatile bool blinking = false;

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action){
    uint32_t now = nrfx_systick_get();

    if(now - last_click_time < 350){
        blinking = !blinking;
    }
    last_click_time = now;

}

void gpiote_init(){
    nrfx_gpiote_init();

    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    nrfx_gpiote_in_init(BUTTON, &config, button_handler);
    nrfx_gpiote_in_event_enable(BUTTON, true);
}

int main(void){
    nrfx_systick_init();
    gpiote_init();

    pwm_init(led_seq[0]);  // первый LED

    uint16_t duty = 0;
    int direction = 1;

    while (1)
    {
        if (blinking)
        {
            pwm_set_duty(duty);

            duty += direction * 10;

            if (duty >= 1000)
            {
                direction = -1;
                led_index = (led_index + 1) % SEQ_LENGTH;
                pwm_switch_led(led_seq[led_index]);
            }

            if (duty == 0)
            {
                direction = 1;
            }

            nrf_delay_ms(10);
        }
        else
        {
            nrf_delay_ms(20);
        }
    }
}