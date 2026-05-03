#include "memory.h"
#include <avr/eeprom.h>

void mem_write_byte(uint16_t addr, uint8_t val) {
    eeprom_update_byte((uint8_t *)(uintptr_t)addr, val);
}

uint8_t mem_read_byte(uint16_t addr) {
    return eeprom_read_byte((const uint8_t *)(uintptr_t)addr);
}

void mem_write_block(uint16_t addr, const void *buf, uint8_t len) {
    eeprom_update_block(buf, (void *)(uintptr_t)addr, len);
}

void mem_read_block(uint16_t addr, void *buf, uint8_t len) {
    eeprom_read_block(buf, (const void *)(uintptr_t)addr, len);
}