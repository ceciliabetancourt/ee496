#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

#define SOS      0   // PA0
#define DOWN     1   // PA1
#define UP       2   // PA2
#define RIGHT    3   // PA3
#define LEFT     4   // PA4
#define ON_OFF   5   // PA5

#define BUTTON_COUNT 6

void buttons_init(void);
void buttons_update(uint16_t elapsed_ms);

uint8_t button_down(uint8_t button);       // 1 while currently held
uint8_t button_pressed(uint8_t button);    // 1 once when button becomes pressed
uint8_t button_released(uint8_t button);   // 1 once when button is released
uint16_t button_hold_time(uint8_t button); // how long currently held, in ms

uint8_t sos_hold_complete(void);           // 1 once when SOS reaches 3000 ms
uint16_t sos_time_remaining(void);         // countdown time remaining in ms

#endif