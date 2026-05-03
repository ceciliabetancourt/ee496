#include "battery.h"
#include <avr/io.h>
#include <util/delay.h>

void battery_init(void) {
    // AVCC reference, ADC channel 6 (PA6 on 1284P — pin 34)
    ADMUX  = (1 << REFS0) | 0x06;

    // Enable ADC, prescaler /128 (16MHz/128 = 125kHz — within 50-200kHz spec)
    ADCSRA = (1 << ADEN)
           | (1 << ADPS2)
           | (1 << ADPS1)
           | (1 << ADPS0);

    // First conversion after enable is inaccurate — discard it
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
}

// Read ADC with oversampling — average 4 samples for stability
uint16_t battery_read_mv(void) {
    uint32_t sum = 0;

    for (uint8_t i = 0; i < 4; i++) {
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC));  // wait for conversion
        sum += ADC;
    }

    uint16_t raw = (uint16_t)(sum / 4);
    return (uint16_t)ADC_TO_BATT_MV(raw);
}

// Map battery voltage to 0-100%
// Li-Ion discharge curve is nonlinear — piecewise linear approximation
uint8_t battery_percent(void) {
    uint16_t mv = battery_read_mv();

    // Clamp to valid range
    if (mv >= BATT_MAX_MV) return 100;
    if (mv <= BATT_MIN_MV) return 0;

    // Piecewise *linear* approximation of Li-Ion curve: (slightly off)
    // 4200-4000 mV → 100-80%  (flat top)
    // 4000-3700 mV → 80-50%   (gradual decline)
    // 3700-3400 mV → 50-20%   (steeper)
    // 3400-2750 mV → 20-0%    (rapid drop)
    if      (mv >= 4000) return 80  + (uint8_t)((mv - 4000) * 20  / 200);
    else if (mv >= 3700) return 50  + (uint8_t)((mv - 3700) * 30  / 300);
    else if (mv >= 3400) return 20  + (uint8_t)((mv - 3400) * 30  / 300);
    else                 return       (uint8_t)((mv - 2750) * 20  / 650);
}

uint8_t battery_is_low(void) {
    return battery_percent() <= BATT_LOW_PERCENT;
}