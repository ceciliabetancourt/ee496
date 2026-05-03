#ifndef WAYPOINT_H
#define WAYPOINT_H

#include <stdint.h>

#define MAX_WAYPOINTS 5

typedef struct {
    float latitude;
    float longitude;
} Waypoint;

void    waypoint_init(void);
uint8_t waypoint_is_valid(uint8_t slot);
uint8_t waypoint_save_to(uint8_t slot, float lat, float lon);
uint8_t waypoint_count(void);
uint8_t waypoint_get(uint8_t slot, Waypoint *wp);
void    waypoint_navigate(float curr_lat, float curr_lon,
                           float wp_lat,   float wp_lon,
                           uint32_t *dist_m, uint16_t *bearing_deg,
                           const char **cardinal);

#endif