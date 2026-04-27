# MCU and clock
MCU = atmega1284p
F_CPU = 7372800UL

# Programmer (change if needed)
PROGRAMMER = usbtiny

# Files
# main.c states.c display.c buttons.c gps.c power.c waypoint.c sos.c battery.c
SRC = main.c states.c display.c buttons.c gps.c power.c waypoint.c sos.c battery.c memory.c
TARGET = main

# Compiler flags
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall

# Build
all: $(TARGET).hex

$(TARGET).elf: $(SRC)
	avr-gcc $(CFLAGS) -o $@ $^ -lm

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

# Flash to chip
flash: $(TARGET).hex
	avrdude -c $(PROGRAMMER) -p m1284p -B 50 -U flash:w:$(TARGET).hex

# Flash main firmware with 5 hardcoded test waypoints pre-loaded
flash-wptest:
	avr-gcc $(CFLAGS) -DWAYPOINT_TEST -o wptest.elf $(SRC) -lm
	avr-objcopy -O ihex -R .eeprom wptest.elf wptest.hex
	avrdude -c $(PROGRAMMER) -p m1284p -B 50 -U flash:w:wptest.hex

# Clean build files
clean:
	rm -f *.elf *.hex *.wptest.elf wptest.hex


# RUN THIS FIRST: avrdude -c usbtiny -p m1284p -B 50 -U lfuse:w:0xE0:m
# THEN MAKE FUSES
# THEN MAKE FLASH
