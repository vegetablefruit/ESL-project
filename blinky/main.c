#include <nrf_gpio.h>
#include <nrf_delay.h>

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

static uint32_t index = 0;

void led_is_on(uint32_t pin)
{
    nrf_gpio_pin_write(pin, 1);
}

void led_is_off(uint32_t pin)
{
    nrf_gpio_pin_write(pin, 0);
}

void all_leds_are_off()
{
    led_is_off(LED_1);
    led_is_off(LED_R);
    led_is_off(LED_G);
    led_is_off(LED_B);
}

int main(void)
{
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_R);
    nrf_gpio_cfg_output(LED_G);
    nrf_gpio_cfg_output(LED_B);
    nrf_gpio_cfg_input(BUTTON, NRF_GPIO_PIN_PULLUP);

    all_leds_are_off();

    while (1)
    {
        // button is pressed
        if ((nrf_gpio_pin_read(BUTTON) == 0))
        {
            led_is_on(led_seq[index]);
            nrf_delay_ms(500);
            led_is_off(led_seq[index]);

            index = (index + 1) % SEQ_LENGTH;
        }
        // button is not pressed
        else
        {
            nrf_delay_ms(50);
        }
    }
}