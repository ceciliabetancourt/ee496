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

    // current_state = STATE_INIT; 
    // On reset go to init, and then inside init make sure all these work. 
    // Also maybe make this function a boolean
}

#define retry_limit 3

// next state logic
void state_machine_run(void) {
    int flag_sos_arming = 0;
    int flag_sos_rx = 0;

    switch (current_state) {
        case STATE_INIT:
            // TODO: 
            // Boot Screen:
            // ! display.c

            // Load Saved Data:
            // ! memory.c

            // State Transition:
            // ! system_init function should turn everything on, on states.c
            if (system_init()) {
                current_state = STATE_IDLE;
            } else {
                current_state = STATE_ERROR;
            }
            break;

        case STATE_IDLE:
            // TODO: 
            // Display Functions to show compass and battery level
            // ! Add to display.c

            // State Transitions:
            // ! add sos_button_press variable/function in buttons.c
            if (sos_button_press == 1) {
                current_state = STATE_GPS_CHECK;
            } else {
                current_state = STATE_IDLE;
            }

            // Deal with later: if both waypoint and SOS actions are possible from idle, you’ll want a clear priority rule.
            break;

        case STATE_GPS_CHECK:
            // TODO: 

            // Use UART to ask for data from GPS:
            if (uart_available()) {
                uart_init(baud rate);
                // ! Maybe all this happens in some type of get_coordinates() function in gps.c
                uart_read_char(); 
                // ?? am I missing anything from the uart file
            }

            // ! probably some display from display.c to say loading from GPS

            // State Transitions:
            // ! Add coordinates_valid in gps.c
            if (coordinates_valid && sos_button_press == 1) {
                current_state = STATE_SOS_ARMING;
            } else {
                current_state = STATE_IDLE;
            }
            break;

        case STATE_SOS_ARMING:
            // TODO: 

            // ! display function for screen flash display.c
            if(flag_sos_arming == 0){
                flag_sos_arming = 1;
            }
            // State Transitions:
            if (sos_time_remaining()) {
                current_state = STATE_SOS_ARMING;
            } else if (sos_hold_complete()) {
                flag_sos_arming = 0;
                current_state = STATE_SOS_FORMAT;
            } else {
                flag_sos_arming = 0;
                current_state = STATE_IDLE;
            }
            break;

            // flag_sos_arming may not be needed but its just a way to know if it is the first time to enter the SOS_ARMING state or not

        case STATE_SOS_FORMAT:
            // TODO: 

            // Use UART to ask for data to MODEM:
            if (uart_available()) {
                uart_init(baud rate);
                // ! Maybe all this happens in some type of set_coordinates() function in sos.c
                uart_read_char(); 
                // ?? am I missing anything from the uart file
            }

            // ! function in sos.c to format
            // State Transitions:
            flag_sos_rx = 0;
            current_state = STATE_SOS_TRANSMIT;
            break;

        case STATE_SOS_TRANSMIT:
            // TODO: 
            // ! function in display.c to say waiting for ack
            // State Transitions:
            if (ack_received) {
                current_state = STATE_SOS_SUCCESS;
            } else if (flag_sos_rx < retry_limit && !ack_received) {
                flag_sos_rx++;
                current_state = STATE_SOS_TRANSMIT;
            }
            // ! add ack_received to sos.c
            else if (flag_sos_rx == retry_limit) {
                current_state = STATE_SOS_FAILURE;
            }
            break;

        case STATE_SOS_SUCCESS:
            // TODO: 
            // ! display.c screen showing success
            // State Transitions:
            current_state = STATE_IDLE;
            break;

        case STATE_SOS_FAILURE:
            // TODO: 

            // State Transitions:

            break;

        case STATE_LOW_POWER:
            // TODO: 

            // State Transitions:

            break;

        case STATE_ERROR:
            // TODO: 

            // State Transitions:

            break;

        case STATE_SHUTDOWN:
            // TODO: 

            // State Transitions:

            break;

        case STATE_WAYPOINT_CONFIRM:
            // TODO: 

            // State Transitions:

            break;

        case STATE_WAYPOINT_SAVED:
            // TODO: 

            // State Transitions:

            break;

        case STATE_WAYPOINT_SELECT_DELETE:
            // TODO: 

            // State Transitions:

            break;
    }
}

// add in logic to be performed in each state