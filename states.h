#ifndef STATES_H
#define STATES_H

#include <stdint.h>

typedef enum {
    STATE_INIT,
    STATE_MENU,
    STATE_BATTERY_CHECK,
    STATE_GPS_VIEW,
    STATE_SOS_CONFIRM,
    STATE_SOS_COUNTDOWN,
    STATE_SOS_HOLD_COUNTDOWN,
    STATE_GPS_CHECK,
    STATE_SOS_ARMING,
    STATE_SOS_FORMAT,
    STATE_SOS_TRANSMIT,
    STATE_SOS_SUCCESS,
    STATE_SOS_FAILURE,
    STATE_LOW_POWER,
    STATE_ERROR,
    STATE_SHUTDOWN,
    STATE_WAYPOINT_SLOT_SELECT,
    STATE_WAYPOINT_OVERWRITE_CONFIRM,
    STATE_WAYPOINT_SAVE,
    STATE_WAYPOINT_SAVED,
    STATE_WAYPOINT_LIST,
    STATE_WAYPOINT_NAV
} DeviceState;

void        system_init(void);
void        state_machine_run(uint16_t elapsed_ms);
void        state_machine_set(DeviceState new_state);
DeviceState state_machine_get(void);
void        state_enter_low_power(DeviceState from_state);

#endif
