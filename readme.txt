================================================================================
EE496 Senior Capstone Design - Spring 2026
Solar-Powered S.O.S. Device
Team: Ashley Ly, Cecilia Betancourt, Mina Gobran
Faculty Advisor: Dr. Allan Weber
University of Southern California
================================================================================

PROJECT OVERVIEW
----------------
This project is a solar-powered satellite SOS beacon designed for hikers and
backpackers in remote areas without cellular coverage. The device uses the
Iridium LEO satellite network via the RockBlock 9603 modem to transmit GPS
coordinates to emergency services upon user activation.


HARDWARE
--------
- Microcontroller:  ATmega1284P DIP-40 (16MHz external crystal)
- GPS Module:       Adafruit Ultimate GPS Breakout v3 (MTK3339)
- Satellite Modem:  RockBlock 9603 (Iridium SBD)
- Display:          Adafruit 1.5" 128x128 Grayscale OLED (SSD1327) via I2C
- Charger:          Adafruit BQ25185 USB/Solar charger with 5V boost
- Solar Panel:      Voltaic Systems P124 (~6V, 200mA peak)
- Battery:          GlobTek Li-Ion 3.7V 1.05Ah


PIN MAPPING (ATmega1284P DIP-40)
---------------------------------
PA0 (pin 40) - SOS button (active LOW, internal pull-up)
PA1 (pin 39) - DOWN button (active LOW, internal pull-up)
PA2 (pin 38) - UP button (active LOW, internal pull-up)
PA3 (pin 37) - RIGHT button (active LOW, internal pull-up)
PA4 (pin 36) - LEFT button (active LOW, internal pull-up)
PA5 (pin 35) - ON/OFF button (active LOW, internal pull-up)
PA6 (pin 34) - Battery voltage ADC (direct connection from BQ25185 BAT pad)
PB5 (pin 6)  - SPI MOSI (ISP programmer)
PB6 (pin 7)  - SPI MISO (ISP programmer)
PB7 (pin 8)  - SPI SCK (ISP programmer)
PC0 (pin 22) - I2C SCL (OLED display)
PC1 (pin 23) - I2C SDA (OLED display)
PD0 (pin 15) - UART0 RX <- GPS TX (9600 baud)
PD1 (pin 16) - UART0 TX -> GPS RX (9600 baud)
PD2 (pin 17) - UART1 RX <- RockBlock TX (19200 baud)
PD3 (pin 18) - UART1 TX -> RockBlock RX (19200 baud)
PD6 (pin 21) - BQ25185 CHG status (input, open-drain active LOW)
RESET (pin 9) - 10k pull-up to VCC


FUSE BITS (set once, required for 16MHz external crystal)
----------------------------------------------------------
lfuse: 0xFF   (external crystal full swing oscillator)
hfuse: 0xD9   (default)
efuse: 0xFF   (default)

Command to set fuses:
avrdude -c usbtiny -p m1284p -U lfuse:w:0xFF:m -U hfuse:w:0xD9:m -U efuse:w:0xFF:m

WARNING: Do NOT use lfuse 0xE0 or 0x62 - these will cause timing failures.


SOURCE FILES
------------
main.c       - Timer1-based 1ms tick, buttons updated every 1ms,
               state machine every 10ms. 200ms power-on delay for stability.
states.c     - 18-state finite state machine (see state machine diagram)
states.h     - State enum and function declarations
buttons.c    - Timer-based debounce, hold detection, SOS 3-second hold logic
buttons.h    - Button pin definitions (PA0-PA5) and function declarations
display.c    - SSD1327 128x128 OLED via I2C, 5x7 font (lowercase only)
display.h    - OLED function declarations
gps.c        - UART0 non-blocking NMEA parser, $GPRMC sentence parsing
gps.h        - GPS_Data struct, coordinates_valid flag
battery.c    - ADC channel 6 (PA6), 4-sample average, piecewise Li-Ion curve
battery.h    - ADC formula, battery percentage thresholds
sos.c        - RockBlock 9603 AT command interface via UART1
sos.h        - sos_send() declaration, ack_received flag
power.c      - Stub: power_init(), power_is_low() always returns 0
power.h      - power function declarations
waypoint.c   - Stub: waypoint_init(), waypoint_save()
waypoint.h   - waypoint function declarations


COMPILE AND FLASH
-----------------
Toolchain: avr-gcc 7.3.0, avrdude 8.0.0 (from Arduino15 packages)

Using Makefile (recommended):
    make flash

Manual compile and flash:
    avr-gcc -mmcu=atmega1284p -DF_CPU=16000000UL -Os -I. -o main.elf \
        main.c states.c buttons.c display.c battery.c gps.c power.c waypoint.c sos.c
    avr-objcopy -O ihex main.elf main.hex
    avrdude -c usbtiny -p m1284p -U flash:w:main.hex


KNOWN LIMITATIONS AND NOTES
----------------------------

1. ROCKBLOCK SOS CONNECTION
   The RockBlock 9603 modem is not physically connected in the current
   prototype. The SOS transmission code in sos.c is fully implemented
   including the complete AT command sequence (AT&F, AT&K0, AT+SBDWT,
   AT+CSQ, AT+SBDIX) and should function correctly once the modem is
   wired to UART1 (PD2/PD3). The physical Molex PicoBlade connector
   procurement was attempted but the correct pre-crimped wires were not
   available. In a production build, wires would be soldered directly
   to the RockBlock connector pins.

2. WAYPOINT FUNCTIONALITY
   The waypoint save and waypoint list screens are implemented in the
   state machine (STATE_WAYPOINT_SAVE, STATE_WAYPOINT_SAVED,
   STATE_WAYPOINT_LIST). The user can navigate to these screens from
   the main menu. Full waypoint navigation (navigating back to a saved
   waypoint) was not implemented due to time constraints and is marked
   as a TODO in states.c.

3. GPS FIX REQUIREMENT
   The GPS module requires a clear view of the sky to acquire a
   satellite fix. Indoor testing will always show "acquiring fix..."
   This is expected behavior. The GPS check during SOS has a 60-second
   timeout before returning to the menu.

4. CHARGING STATUS
   The BQ25185 CHG pad connection to PD6 is optional. Without it the
   charging status on the battery screen will not reflect actual
   charging state. The orange LED on the BQ25185 board is the most
   reliable indicator of charging status.

5. BATTERY VOLTAGE DIVIDER
   The battery voltage is read directly from the BQ25185 BAT pad to
   PA6 with no voltage divider. The ADC formula uses 5000mV reference.
   If a voltage divider is added in a future revision, we must update
   ADC_TO_BATT_MV in battery.h accordingly.

6. FONT LIMITATION
   The custom 5x7 font in display.c contains only lowercase letters
   (a-z), digits (0-9), and the following symbols: : . ! > and space.
   All display strings must use lowercase only or characters will
   not render.


STATE MACHINE SUMMARY
---------------------
STATE_INIT              -> initializes all subsystems -> STATE_MENU
STATE_MENU              -> home screen, 4 menu items, SOS hold -> confirm
STATE_BATTERY_CHECK     -> voltage, percent, status, charging
STATE_GPS_VIEW          -> live coordinates and UTC time
STATE_SOS_CONFIRM       -> "are you sure?" confirmation screen
STATE_SOS_COUNTDOWN     -> 3 second countdown before transmitting
STATE_GPS_CHECK         -> waits for valid GPS fix (60s timeout)
STATE_SOS_ARMING        -> GPS valid, preparing to format message
STATE_SOS_FORMAT        -> calls sos_send(), blocking up to 60s
STATE_SOS_TRANSMIT      -> (legacy, redirects to SOS_FORMAT)
STATE_SOS_SUCCESS       -> "help is coming!" displayed 3 seconds
STATE_SOS_FAILURE       -> "send failed", option to retry or cancel
STATE_LOW_POWER         -> battery < 10%, enters PWR_SAVE sleep
STATE_ERROR             -> system init failed, attempts recovery
STATE_SHUTDOWN          -> deep sleep, wakes on PA0 (SOS) or PA5 (PWR)
STATE_WAYPOINT_SAVE     -> save current GPS location
STATE_WAYPOINT_SAVED    -> confirmation message, returns to menu
STATE_WAYPOINT_LIST     -> scroll through saved waypoints


BUTTON REFERENCE
----------------
From any screen:
  SOS (PA0) held 3s   -> triggers SOS confirmation sequence
  PWR (PA5) press     -> shutdown (from menu) or wake (from sleep)

Navigation:
  UP   (PA2)          -> scroll up / previous item
  DOWN (PA1)          -> scroll down / next item
  RIGHT (PA3)         -> select / confirm
  LEFT  (PA4)         -> back / cancel
