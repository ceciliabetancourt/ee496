# MCU and clock
MCU = atmega1284p
F_CPU = 20000000UL

# Programmer (change if needed)
PROGRAMMER = usbtiny

# Files
SRC = test.c
TARGET = main

# Compiler flags
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall

# Build
all: $(TARGET).hex

$(TARGET).elf: $(SRC)
	avr-gcc $(CFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

# Flash to chip
flash: $(TARGET).hex
	avrdude -c $(PROGRAMMER) -p m1284p -B 50 -U flash:w:$(TARGET).hex

# Clean build files
clean:
	rm -f *.elf *.hex


# RUN THIS FIRST: avrdude -c usbtiny -p m1284p -B 50 -U lfuse:w:0xE0:m
# THEN MAKE FUSES
# THEN MAKE FLASH
