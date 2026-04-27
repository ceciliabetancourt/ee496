#include "waypoint.h"
#include "memory.h"
#include <math.h>

// EEPROM layout:
// 0: magic byte (0xA6)
// 1: valid_mask (bitmask, bit i = slot i is occupied)
// 2+: slot data, 8 bytes each (4 lat + 4 lon)
#define EEPROM_MAGIC_ADDR   0
#define EEPROM_VALID_ADDR   1
#define EEPROM_WP_BASE_ADDR 2
#define EEPROM_MAGIC_VAL    0xA6   // bumped from 0xA5 to force re-init

#define EARTH_RADIUS_M  6371000.0f
#define DEG_TO_RAD      (M_PI / 180.0f)

static uint8_t  valid_mask = 0;
static Waypoint wp_cache[MAX_WAYPOINTS];

void waypoint_init(void) {
#ifdef WAYPOINT_TEST
    // Load 5 hardcoded locations near lat:34.0206 lon:-118.289 (Los Angeles)
    static const float test_lats[5] = { 34.0320f, 34.0450f, 34.0060f, 33.9940f, 34.0650f };
    static const float test_lons[5] = {-118.2890f,-118.3150f,-118.2650f,-118.3200f,-118.3050f};
    for (uint8_t i = 0; i < MAX_WAYPOINTS; i++)
        waypoint_save_to(i, test_lats[i], test_lons[i]);
    return;
#endif
    if (mem_read_byte(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VAL) {
        valid_mask = 0;
        mem_write_byte(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
        mem_write_byte(EEPROM_VALID_ADDR, 0);
        return;
    }
    valid_mask = mem_read_byte(EEPROM_VALID_ADDR);
    for (uint8_t i = 0; i < MAX_WAYPOINTS; i++) {
        if (valid_mask & (1 << i)) {
            uint16_t addr = EEPROM_WP_BASE_ADDR + i * sizeof(Waypoint);
            mem_read_block(addr, &wp_cache[i], sizeof(Waypoint));
        }
    }
}

uint8_t waypoint_is_valid(uint8_t slot) {
    if (slot >= MAX_WAYPOINTS) return 0;
    return (valid_mask >> slot) & 1;
}

uint8_t waypoint_save_to(uint8_t slot, float lat, float lon) {
    if (slot >= MAX_WAYPOINTS) return 0;
    wp_cache[slot].latitude  = lat;
    wp_cache[slot].longitude = lon;
    uint16_t addr = EEPROM_WP_BASE_ADDR + slot * sizeof(Waypoint);
    mem_write_block(addr, &wp_cache[slot], sizeof(Waypoint));
    valid_mask |= (1 << slot);
    mem_write_byte(EEPROM_VALID_ADDR, valid_mask);
    return 1;
}

uint8_t waypoint_count(void) {
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < MAX_WAYPOINTS; i++)
        if (valid_mask & (1 << i)) cnt++;
    return cnt;
}

uint8_t waypoint_get(uint8_t slot, Waypoint *wp) {
    if (!waypoint_is_valid(slot)) return 0;
    *wp = wp_cache[slot];
    return 1;
}

void waypoint_navigate(float curr_lat, float curr_lon,
                        float wp_lat,   float wp_lon,
                        uint32_t *dist_m, uint16_t *bearing_deg,
                        const char **cardinal) {
    float phi1 = curr_lat * DEG_TO_RAD;
    float phi2 = wp_lat   * DEG_TO_RAD;
    float dphi = (wp_lat  - curr_lat) * DEG_TO_RAD;
    float dlam = (wp_lon  - curr_lon) * DEG_TO_RAD;

    float a = sinf(dphi/2)*sinf(dphi/2) +
              cosf(phi1)*cosf(phi2)*sinf(dlam/2)*sinf(dlam/2);
    float d = 2.0f * EARTH_RADIUS_M * atan2f(sqrtf(a), sqrtf(1.0f - a));
    *dist_m = (uint32_t)(d + 0.5f);

    float y = sinf(dlam) * cosf(phi2);
    float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(dlam);
    float bearing = atan2f(y, x) * (180.0f / M_PI);
    if (bearing < 0.0f) bearing += 360.0f;
    *bearing_deg = (uint16_t)bearing;

    static const char *cards[] = {"n","ne","e","se","s","sw","w","nw"};
    *cardinal = cards[((uint16_t)(bearing + 22.5f) / 45) % 8];
}
