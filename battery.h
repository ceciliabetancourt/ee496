#ifndef BATTERY_H
#define BATTERY_H
 
#include <stdint.h>
 
// Physical battery limits
#define BATT_MAX_MV     4200
#define BATT_MIN_MV     2750
 
// ADC is 10-bit (0-1023), AVCC = 5000mV
// Direct connection (no divider) — Vbatt = Vadc directly
// Vbatt(mV) = (ADC_raw / 1023.0) * 5000
#define ADC_TO_BATT_MV(raw)   ((uint32_t)(raw) * 5000UL / 1023UL)
 
// Percentage thresholds
#define BATT_WARN_PERCENT   20
#define BATT_LOW_PERCENT    10
 
void     battery_init(void);
uint16_t battery_read_mv(void);
uint8_t  battery_percent(void);
uint8_t  battery_is_low(void);
 
#endif
 