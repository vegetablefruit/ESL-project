#ifndef NRF_BUTTON_H
#define NRF_BUTTON_H

#include <stdbool.h>
#include <nrfx_gpiote.h>

#define DEBOUNCE_MS 50
#define DOUBLE_CLICK_MS 400
#define LONG_CLICK_MS 500

#define LED_ACTIVE_LOW 1
extern uint32_t hue;
extern uint32_t sat;
extern uint32_t val;


typedef enum
{
    MODE_NONE = 0,
    MODE_HUE,
    MODE_SAT,
    MODE_VAL
} input_mode_t;

typedef enum
{
    BUTTON_OFF,
    BUTTON_ON,
    BUTTON_LONG
} button_states_t;


void button_init();

void button_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

void led_on_gpio(uint32_t pin);
void led_off_gpio(uint32_t pin);

void debounce_timer_handler(void *p_context);
void double_click_timer_handler(void *p_context);
void long_click_timer_handler(void *p_context);
void main_timer_handler(void *p_context);
void mode_button_handler(void *p_context);


#endif