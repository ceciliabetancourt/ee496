#ifndef STATES_H
#define STATES_H

#include <stdint.h>

typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_MENU,
    STATE_BATTERY_CHECK,
    STATE_GPS_CHECK,
    STATE_SOS_ARMING,
    STATE_SOS_FORMAT,
    STATE_SOS_TRANSMIT,
    STATE_SOS_SUCCESS,
    STATE_SOS_FAILURE,
    STATE_LOW_POWER,
    STATE_ERROR,
    STATE_SHUTDOWN,
    STATE_WAYPOINT_CONFIRM,
    STATE_WAYPOINT_SAVED,
    STATE_WAYPOINT_SELECT_DELETE
} DeviceState;

void        system_init(void);
void        state_machine_run(uint16_t elapsed_ms);
void        state_machine_set(DeviceState new_state);
DeviceState state_machine_get(void);
void        state_enter_low_power(DeviceState from_state);

#endif
