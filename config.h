// repeated constants we will be using

#ifndef CONFIG_H
#define CONFIG_H

#define SOS_HOLD_TIME_MS         3000
#define GPS_FIX_TIMEOUT_MS       15000
#define SOS_RETRY_LIMIT          3

#endif

// ATMEGA328P MAPPING
// PINS
// 1    Reset (5V)
// 2    PD0: RX1 (modem)
// 3    PD1: TX1 (modem)
// 4    PD2: unused
// 5    PD3: unused
// 6    PD4: unused
// 7    VCC
// 8    GND
// 9    PB6: programmer connection (temp)
// 10   PB7: RX2 (gps)
// 11   PD5: TX2 (gps)
// 12   PD6: unused
// 13   PD7: unused
// 14   PB0: unused
// 15   PB1: unused
// 16   PB2: CS/Slave Select (OLED)
// 17   PB3: MOSI (OLED)
// 18   PB4: MISO (OLED)
// 19   PB5: SPI Clock (OLED)
// 20   AVCC: unused
// 21   AREF: unused
// 22   GND
// 23   PC0: button on/off
// 24   PC1: button left
// 25   PC2: button right
// 26   PC3: button up
// 27   PC4: button down
// 28   PC5: button sos
