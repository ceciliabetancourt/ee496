#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  128
#define OLED_BUFFER_SIZE ((OLED_WIDTH * OLED_HEIGHT) / 2)
#define OLED_I2C_ADDR 0x3D

void oled_init(void);
void oled_clear(uint8_t shade);
void oled_update(void);
void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t shade);

#endif
