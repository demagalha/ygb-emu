#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>

int inc(uint8_t* reg);
int dec(uint8_t* reg);
int rlca(uint8_t* reg);
int rrca(uint8_t* reg);
int rla(uint8_t* reg);
int rra(uint8_t* reg);
int daa(uint8_t* reg);
int add_16(uint16_t* reg, uint16_t value);
int add(uint8_t* reg, uint8_t value);
int adc(uint8_t* reg, uint8_t value);
int sub(uint8_t* reg, uint8_t value);
int sbc(uint8_t* reg, uint8_t value);
int and_(uint8_t* reg, uint8_t value);
int xor_(uint8_t* reg, uint8_t value);
int or_(uint8_t* reg, uint8_t value);
int cp(uint8_t reg, uint8_t value);
int rst(uint8_t addr);

#endif