#include "display.h"

// declare functions from display.c
void draw_string(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);

int main(void) {
    oled_init();

    // white background
    oled_clear(0xF);

    // scale factor (bigger = more visible)
    uint8_t scale = 1;

    // string
    const char *text = "sivan and cecilia!";

    // calculate width: 6 pixels per char
    uint8_t text_width = 6 * scale * 7; // 7 chars

    // center coordinates
    //x = (128 - text_width) / 2
    uint8_t x = 7;
    uint8_t y = (128 - (7 * scale)) / 2;

    // draw text (black)
    draw_string(x, y, text, scale, 0x0);

    oled_update();

    while (1) {
    }
}