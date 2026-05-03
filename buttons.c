#include "buttons.h"
#include <avr/io.h>
#include <stdint.h>

#define DEBOUNCE_MS   30
#define SOS_HOLD_MS   3000

typedef struct {
    uint8_t stable_down;        // debounced current state: 1 = pressed
    uint8_t last_stable_down;   // previous debounced state
    uint8_t press_event;        // latched one-shot event
    uint8_t release_event;      // latched one-shot event
    uint8_t hold_event;         // latched one-shot event for long hold
    uint16_t debounce_time;     // ms candidate state has been changing
    uint16_t hold_time;         // ms held down in stable pressed state
} ButtonState;

static ButtonState buttons[BUTTON_COUNT];

// returns 1 if button is pressed from raw hardware
static uint8_t button_raw(uint8_t button) {
    switch (button) {
        case SOS:    return !(PINA & (1 << PA0));
        case DOWN:   return !(PINA & (1 << PA1));
        case UP:     return !(PINA & (1 << PA2));
        case RIGHT:  return !(PINA & (1 << PA3));
        case LEFT:   return !(PINA & (1 << PA4));
        case ON_OFF: return !(PINA & (1 << PA5));
        default:     return 0;
    }
}

void buttons_init(void) {
    // make sure buttons are set to inputs
    DDRA &= ~((1 << PA0) | (1 << PA1) | (1 << PA2) | (1 << PA3) | (1 << PA4) | (1 << PA5));

    // enable pull ups
    PORTA |= ((1 << PA0) | (1 << PA1) | (1 << PA2) | (1 << PA3) | (1 << PA4) | (1 << PA5));

    // initialize button object
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].stable_down = 0;
        buttons[i].last_stable_down = 0;
        buttons[i].press_event = 0;
        buttons[i].release_event = 0;
        buttons[i].hold_event = 0;
        buttons[i].debounce_time = 0;
        buttons[i].hold_time = 0;
    }
}

void buttons_update(uint16_t elapsed_ms) {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        uint8_t raw = button_raw(i);

        // Save previous stable state
        buttons[i].last_stable_down = buttons[i].stable_down;

        // Debounce logic
        if (raw != buttons[i].stable_down) {
            if (buttons[i].debounce_time < DEBOUNCE_MS) {
                buttons[i].debounce_time += elapsed_ms;
                if (buttons[i].debounce_time > DEBOUNCE_MS) {
                    buttons[i].debounce_time = DEBOUNCE_MS;
                }
            }

            if (buttons[i].debounce_time >= DEBOUNCE_MS) {
                buttons[i].stable_down = raw;
                buttons[i].debounce_time = 0;
            }
        } else {
            buttons[i].debounce_time = 0;
        }

        // Edge events
        if (buttons[i].stable_down && !buttons[i].last_stable_down) {
            buttons[i].press_event = 1;
            buttons[i].hold_time = 0;
            buttons[i].hold_event = 0;
        }

        if (!buttons[i].stable_down && buttons[i].last_stable_down) {
            buttons[i].release_event = 1;
            buttons[i].hold_time = 0;
            buttons[i].hold_event = 0;
        }

        // Hold timer
        if (buttons[i].stable_down) {
            if (buttons[i].hold_time < 65535 - elapsed_ms) {
                buttons[i].hold_time += elapsed_ms;
            }
            
            // sos ONLY
            if (i == SOS && buttons[i].hold_time >= SOS_HOLD_MS && !buttons[i].hold_event) {
                buttons[i].hold_event = 1;
            }
        }
    }
}

// returns 1 while button is being held down
uint8_t button_down(uint8_t button) {
    return buttons[button].stable_down;
}

// single actions (single press)
uint8_t button_pressed(uint8_t button) {
    if (buttons[button].press_event) {
        buttons[button].press_event = 0;
        return 1;
    }

    return 0;
}

// returns 1 when buttons is released -- is necessary or no?
uint8_t button_released(uint8_t button) {
    if (buttons[button].release_event) {
        buttons[button].release_event = 0;
        return 1;
    }

    return 0;
}

// returns how long a button is being held for
uint16_t button_hold_time(uint8_t button) {
    return buttons[button].hold_time;
}

// returns 1 once the sos_button is held for 3 seconds
uint8_t sos_hold_complete(void) {
    if (buttons[SOS].hold_event) {
        buttons[SOS].hold_event = 0;
        return 1;
    }
    return 0;
}

// countdown time for output to OLED
uint16_t sos_time_remaining(void) {
    if (!buttons[SOS].stable_down) {
        return SOS_HOLD_MS;
    }

    if (buttons[SOS].hold_time >= SOS_HOLD_MS) {
        return 0;
    }

    return (SOS_HOLD_MS - buttons[SOS].hold_time);
}
