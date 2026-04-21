#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  128
#define OLED_BUFFER_SIZE ((OLED_WIDTH * OLED_HEIGHT) / 2)  // 4-bit grayscale, 2 pixels per byte

// Default Adafruit SSD1327 I2C address
#define OLED_I2C_ADDR 0x3D

void oled_init(void);
void oled_clear(uint8_t shade);
void oled_update(void);
void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t shade);
void display_draw_char(uint8_t x, uint8_t y, char c);
void display_draw_string(uint8_t x, uint8_t y, const char *str);

#endif

/// given the display is 128 by 128, we can place approximately 21 characters (given they are 5x7 characters)
/// on one line