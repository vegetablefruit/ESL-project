#ifndef NRF_HSV_H
#define NRF_HSV_H

#include <stdint.h>

extern uint16_t rgb_r;
extern uint16_t rgb_g;
extern uint16_t rgb_b;

void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v);

#endif