#include "display.h"
#include "gps.h"
#include <stdlib.h>
#include <string.h>

int main(void) {
    oled_init();
    gps_init();

    GPS_Data gps;
    char line[24];
    char val[12];

    while (1) {
        if (gps_read(&gps)) {
            oled_clear(0x0);

            dtostrf(gps.latitude,  9, 4, val);
            strcpy(line, "lat:");
            strcat(line, val);
            draw_string(2, 52, line, 1, 0xF);

            dtostrf(gps.longitude, 9, 4, val);
            strcpy(line, "lon:");
            strcat(line, val);
            draw_string(2, 65, line, 1, 0xF);
        } else {
            oled_clear(0x0);
            draw_string(28, 61, "no fix", 1, 0xF);
        }

        oled_update();
    }
}
