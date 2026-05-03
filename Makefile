MCU = atmega1284p
F_CPU = 7372800UL

PROGRAMMER = usbtiny

# main.c states.c display.c buttons.c gps.c power.c waypoint.c sos.c battery.c
SRC = main.c states.c display.c buttons.c gps.c power.c waypoint.c sos.c battery.c memory.c
TARGET = main

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall

all: $(TARGET).hex

$(TARGET).elf: $(SRC)
	avr-gcc $(CFLAGS) -o $@ $^ -lm

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

flash: $(TARGET).hex
	avrdude -c $(PROGRAMMER) -p m1284p -B 50 -U flash:w:$(TARGET).hex

flash-wptest:
	avr-gcc $(CFLAGS) -DWAYPOINT_TEST -o wptest.elf $(SRC) -lm
	avr-objcopy -O ihex -R .eeprom wptest.elf wptest.hex
	avrdude -c $(PROGRAMMER) -p m1284p -B 50 -U flash:w:wptest.hex

clean:
	rm -f *.elf *.hex *.wptest.elf wptest.hex