#include "power.h"
#include "battery.h"

void power_init(void) {}

uint8_t power_is_low(void) {
    return battery_is_low();
}