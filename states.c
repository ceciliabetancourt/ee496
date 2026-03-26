#include "states.h"
#include "gps.h"
#include "display.h"
#include "buttons.h"
#include "power.h"
#include "waypoint.h"
#include "sos.h"

static DeviceState current_state = STATE_INIT;

void state_machine_set(DeviceState new_state) {
    current_state = new_state;
}

DeviceState state_machine_get(void) {
    return current_state;
}

void system_init(void) {
    display_init();
    buttons_init();
    gps_init();
    power_init();
    waypoint_init();
    sos_init();

    current_state = STATE_INIT;
}


// next state logic
void state_machine_run(void) {
    switch (current_state) {
        case STATE_INIT:

        case STATE_GPS_CHECK:

        case STATE_IDLE:

        case STATE_WAYPOINT_CONFIRM:

        case STATE_WAYPOINT_SAVED:

        case STATE_SOS_ARMING:

        case STATE_SOS_FORMAT:

        case STATE_SOS_TRANSMIT:

        case STATE_SOS_SUCCESS:

        case STATE_SOS_FAILURE:

        case STATE_LOW_POWER:

        case STATE_ERROR:

        case STATE_SHUTDOWN:

        case STATE_WAYPOINT_SELECT_DELETE:
    }
}

// add in logic to be performed in each state