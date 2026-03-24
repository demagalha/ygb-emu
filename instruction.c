#include "instruction.h"
#include "cpu.h"
#include "memory.h"

int inc(uint8_t* reg) {
    uint8_t result = *reg + 1;

    WRITE_FLAG(FLAG_Z, result == 0);
    CLEAR_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, (*reg & 0x0F) == 0x0F);

    *reg = result;
    return 4;
}

int dec(uint8_t* reg) {
    uint8_t result = *reg - 1;

    WRITE_FLAG(FLAG_Z, result == 0);
    SET_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, (*reg & 0x0F) == 0x00);

    *reg = result;
    return 4;
}

// only to be used with cpu.a
int rlca(uint8_t* reg) {
    uint8_t oldA = *reg;
    *reg = (*reg << 1) | (*reg >> 7);
    WRITE_FLAG(FLAG_C, oldA & 0x80);
    CLEAR_FLAG(FLAG_Z | FLAG_N | FLAG_H);
    return 4;
}

int rrca(uint8_t* reg) {
    uint8_t old_bit0 = *reg & 0x01;
    *reg = (*reg >> 1) | (old_bit0 << 7);
    WRITE_FLAG(FLAG_C, old_bit0);
    CLEAR_FLAG(FLAG_Z | FLAG_N | FLAG_H);
    return 4;
}

int rla(uint8_t* reg) {
    uint8_t old_carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint8_t new_carry = (*reg & 0x80) ? 1 : 0;

    *reg = (*reg << 1) | old_carry;

    WRITE_FLAG(FLAG_C, new_carry);
    CLEAR_FLAG(FLAG_Z | FLAG_N | FLAG_H);

    return 4;
}

int rra(uint8_t* reg) {
    uint8_t old_carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint8_t old_bit0 = *reg & 0x01;

    *reg = (*reg >> 1) | (old_carry << 7);

    CLEAR_FLAG(FLAG_Z | FLAG_N | FLAG_H);

    if (old_bit0)
        SET_FLAG(FLAG_C);
    else
        CLEAR_FLAG(FLAG_C);

    return 4;
}

int daa(uint8_t* reg) {
    uint8_t correction = 0;
    
    if (CHECK_FLAG(FLAG_N)) {
        if (CHECK_FLAG(FLAG_H)) correction |= 0x06;
        if (CHECK_FLAG(FLAG_C)) correction |= 0x60;
        *reg -= correction;
    } else {
        if (CHECK_FLAG(FLAG_H) || (*reg & 0x0F) > 9) correction |= 0x06;
        if (CHECK_FLAG(FLAG_C) || *reg > 0x99) correction |= 0x60;
        *reg += correction;
    }

    WRITE_FLAG(FLAG_H, 0);
    WRITE_FLAG(FLAG_C, correction & 0x60);
    WRITE_FLAG(FLAG_Z, *reg == 0);

    return 4;
}

int add_16(uint16_t* reg, uint16_t value) {
    uint32_t result = *reg + value;

    WRITE_FLAG(FLAG_H, ((*reg & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF);
    WRITE_FLAG(FLAG_C, result > 0xFFFF);
    CLEAR_FLAG(FLAG_N);

    *reg = result & 0xFFFF;
    return 8;
}

int add(uint8_t* reg, uint8_t value) {
    uint16_t result = *reg + value;
    WRITE_FLAG(FLAG_Z, (result & 0xFF) == 0);
    CLEAR_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, ((*reg & 0x0F) + (value & 0x0F)) > 0x0F);
    WRITE_FLAG(FLAG_C, result > 0xFF);

    *reg = result & 0xFF;
    return 4;
}

int adc(uint8_t* reg, uint8_t value) {
    uint16_t carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint16_t result = *reg + value + carry;

    WRITE_FLAG(FLAG_Z, (result & 0xFF) == 0);
    CLEAR_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, ((*reg & 0xF) + (value & 0xF) + carry) > 0xF);
    WRITE_FLAG(FLAG_C, result > 0xFF);

    *reg = result & 0xFF;
    return 4;
}

int sub(uint8_t* reg, uint8_t value) {
    uint8_t result = *reg - value;
    WRITE_FLAG(FLAG_Z, result == 0);
    SET_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, (*reg & 0x0F) < (value & 0x0F));
    WRITE_FLAG(FLAG_C, *reg < value);
    *reg = result;
    return 4;
}

int sbc(uint8_t* reg, uint8_t value) {
    uint8_t carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    uint16_t result = *reg - value - carry;

    WRITE_FLAG(FLAG_Z, (result & 0xFF) == 0);
    SET_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, (*reg & 0x0F) < ((value & 0x0F) + carry));
    WRITE_FLAG(FLAG_C, *reg < (value + carry));

    *reg = result & 0xFF;
    return 4;
}

int and_(uint8_t* reg, uint8_t value) {
    *reg &= value;
    WRITE_FLAG(FLAG_Z, !*reg);
    CLEAR_FLAG(FLAG_C | FLAG_N);
    SET_FLAG(FLAG_H);
    return 4;
}

int xor_(uint8_t*reg, uint8_t value) {
    *reg ^= value;
    WRITE_FLAG(FLAG_Z, !*reg);
    CLEAR_FLAG(FLAG_C | FLAG_N | FLAG_H);
    return 4;
}

int or_(uint8_t* reg, uint8_t value) {
    *reg |= value;
    WRITE_FLAG(FLAG_Z, !*reg);
    CLEAR_FLAG(FLAG_C | FLAG_N | FLAG_H);
    return 4;
}

int cp(uint8_t reg, uint8_t value) {
    uint8_t result = reg - value;

    WRITE_FLAG(FLAG_Z, result == 0);
    SET_FLAG(FLAG_N);
    WRITE_FLAG(FLAG_H, (reg & 0x0F) < (value & 0x0F));
    WRITE_FLAG(FLAG_C, reg < value);

    return 4;
}

int rst(uint8_t addr) {
    cpu.sp--;
    memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
    cpu.sp--;
    memory_write8(cpu.sp, cpu.pc & 0xFF);
    cpu.pc = addr;
    return 16;
}
