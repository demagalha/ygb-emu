#include "instruction_cb.h"
#include "cpu.h"

int rlc(uint8_t* reg) {
    uint8_t old_bit7 = (*reg & 0x80) >> 7;

    *reg = (*reg << 1) | old_bit7;

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit7);

    return 8;
}

int rrc(uint8_t* reg) {
    uint8_t old_bit0 = *reg & 0x01;

    *reg = (*reg >> 1) | (old_bit0 << 7);

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit0);

    return 8;
}

int rl(uint8_t* reg) {
    uint8_t old_carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint8_t old_bit7 = (*reg & 0x80) >> 7;

    *reg = (*reg << 1) | old_carry;

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit7);

    return 8;
}

int rr(uint8_t* reg) {
    uint8_t old_carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint8_t old_bit0 = *reg & 0x01;

    *reg = (*reg >> 1) | (old_carry << 7);

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit0);

    return 8;
}

int sla(uint8_t* reg) {
    uint8_t old_bit7 = (*reg & 0x80) >> 7;

    *reg <<= 1;

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit7);

    return 8;
}

int sra(uint8_t* reg) {
    uint8_t old_bit0 = *reg & 0x01;
    uint8_t old_bit7 = *reg & 0x80;

    *reg = (*reg >> 1) | old_bit7;

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit0);

    return 8;
}

int swap(uint8_t* reg) {
    *reg = (*reg >> 4) | (*reg << 4);
    WRITE_FLAG(FLAG_Z, !*reg);
    CLEAR_FLAG(FLAG_N | FLAG_H | FLAG_C);
    return 8;
}

int srl (uint8_t* reg) {
    uint8_t old_bit0 = *reg & 0x01;
    *reg >>= 1;

    WRITE_FLAG(FLAG_Z, *reg == 0);
    CLEAR_FLAG(FLAG_N | FLAG_H);
    WRITE_FLAG(FLAG_C, old_bit0);

    return 8;
}

int bit(uint8_t reg, uint8_t bit) {
    WRITE_FLAG(FLAG_Z, !(reg & (1 << bit)));
    CLEAR_FLAG(FLAG_N);
    SET_FLAG(FLAG_H);
    return 8;
}

int res(uint8_t* reg, uint8_t bit) {
    *reg &= ~(1 << bit);
    return 8;
}

int set(uint8_t* reg, uint8_t bit) {
    *reg |= (1 << bit);
    return 8;
}