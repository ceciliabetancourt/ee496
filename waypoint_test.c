#include "display.h"
#include "waypoint.h"
#include <util/delay.h>

// 5 locations near lat:34.0206 lon:-118.289 (Los Angeles area)
// distances range from ~1.5 km to ~5 km in various directions
static const float test_lats[5] = {
    34.0320f,   // slot 0 — ~1.6 km N
    34.0450f,   // slot 1 — ~3.3 km NW
    34.0060f,   // slot 2 — ~1.6 km SE
    33.9940f,   // slot 3 — ~3.1 km SW
    34.0650f,   // slot 4 — ~4.9 km NNW
};
static const float test_lons[5] = {
    -118.2890f,
    -118.3150f,
    -118.2650f,
    -118.3200f,
    -118.3050f,
};

int main(void) {
    oled_init();
    waypoint_init();

    oled_clear(0x0);
    draw_string(2, 45, "loading wps...", 1, 0xF);
    oled_update();
    _delay_ms(1000);

    for (uint8_t i = 0; i < 5; i++) {
        waypoint_save_to(i, test_lats[i], test_lons[i]);
    }

    oled_clear(0x0);
    draw_string(2, 35, "5 wps loaded!", 1, 0xF);
    draw_string(2, 55, "now flash main", 1, 0xF);
    oled_update();

    while (1) {}
}
