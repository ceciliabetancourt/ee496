#include "states.h"
#include "buttons.h"
#include <util/delay.h>

#define LOOP_MS 10

int main(void) {
    system_init();

    while (1) {
        buttons_update(LOOP_MS);
        state_machine_run(LOOP_MS);
        _delay_ms(LOOP_MS);
    }

    return 0;
}
