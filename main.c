// initialization for the machine as well as the general loop where 
// everything will be run

#include "states.h"
#include "gps.h"
#include "buttons.h"
#include "power.h"
#include "display.h"

int main(void) {
    // initialize system (system_init in states.c)

    while (1) {
        // check buttons
        // check gps
        // check power

        // run state machine with given parameters
        // update display as necessary
    }

    return 0;
}