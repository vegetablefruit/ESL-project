#include "nrf_hsv.h"

uint16_t rgb_r;
uint16_t rgb_g;
uint16_t rgb_b;


void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint8_t region = h / 60;
    uint16_t remainder = (h % 60) * 255 / 60;

    uint16_t p = (uint16_t)v * (255 - s) / 255;
    uint16_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder) / 255) / 255;
    uint16_t t = (uint16_t)v * (255 - ((uint16_t)s * (255 - remainder)) / 255) / 255;

    switch (region)
    {
    case 0:
        rgb_r = v;
        rgb_g = t;
        rgb_b = p;
        break;

    case 1:
        rgb_r = q;
        rgb_g = v;
        rgb_b = p;
        break;

    case 2:
        rgb_r = p;
        rgb_g = v;
        rgb_b = t;
        break;

    case 3:
        rgb_r = p;
        rgb_g = q;
        rgb_b = v;
        break;

    case 4:
        rgb_r = t;
        rgb_g = p;
        rgb_b = v;
        break;

    default:
        rgb_r = v;
        rgb_g = p;
        rgb_b = q;
        break;
    }
}