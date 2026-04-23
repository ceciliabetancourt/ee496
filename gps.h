#ifndef GPS_H
#define GPS_H

#include <stdint.h>

typedef struct {
    float latitude;
    float longitude;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} GPS_Data;

void    gps_init(void);
uint8_t gps_read(GPS_Data *data);

#endif
