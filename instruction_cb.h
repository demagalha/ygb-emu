#ifndef INSTRUCTION_CB_H
#define INSTRUCTION_CB_H

#include <stdint.h>

int rlc(uint8_t* reg);
int rrc(uint8_t* reg);
int rl(uint8_t* reg);
int rr(uint8_t* reg);
int sla(uint8_t* reg);
int sra(uint8_t* reg);
int swap(uint8_t* reg);
int srl(uint8_t* reg);
int bit(uint8_t reg, uint8_t bit);
int res(uint8_t* reg, uint8_t bit);
int set(uint8_t* reg, uint8_t bit);


#endif