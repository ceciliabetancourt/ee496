#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

void    mem_write_byte(uint16_t addr, uint8_t val);
uint8_t mem_read_byte(uint16_t addr);
void    mem_write_block(uint16_t addr, const void *buf, uint8_t len);
void    mem_read_block(uint16_t addr, void *buf, uint8_t len);

#endif