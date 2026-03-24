#include "cpu.h"
#include "memory.h"
#include "instruction.h"
#include "instruction_cb.h"
#include <stdio.h>

CPU cpu;

void cpu_reset() {
    cpu.af = 0x01B0;
    cpu.bc = 0x0013;
    cpu.de = 0x00D8;
    cpu.hl = 0x014D;
    cpu.sp = 0xFFFE;
    cpu.pc = 0x0100; // post boot, game entry point
    cpu.halted = 0;
    cpu.ime = 0;
    cpu.ime_pending = 0;
}

uint8_t fetch8() {
    return memory_read8(cpu.pc++);
}

uint16_t fetch16() {
    uint8_t lo = fetch8();
    uint8_t hi = fetch8();
    return (hi << 8) | lo;
}

void print_flags() {
    printf("Flags: Z=%d N=%d H=%d C=%d\n",
        (cpu.f & FLAG_Z) != 0,
        (cpu.f & FLAG_N) != 0,
        (cpu.f & FLAG_H) != 0,
        (cpu.f & FLAG_C) != 0);
}

int handle_interrupts(void) {
    uint8_t ie    = memory_read8(0xFFFF); // IE
    uint8_t iflag = memory_read8(0xFF0F); // IF
    uint8_t fired = ie & iflag & 0x1F;

    if (fired == 0) {
        return 0;
    }

    cpu.halted = 0;

    // Interrupt pending but IME disabled
    if (!cpu.ime) {
        return 0;
    }

    // Service interrupt
    cpu.ime = 0;

    uint16_t vector;
    uint8_t bit;

    if (fired & 0x01) { vector = 0x40; bit = 0x01; } // VBlank
    else if (fired & 0x02) { vector = 0x48; bit = 0x02; } // LCD STAT
    else if (fired & 0x04) { vector = 0x50; bit = 0x04; } // Timer
    else if (fired & 0x08) { vector = 0x58; bit = 0x08; } // Serial
    else { vector = 0x60; bit = 0x10; } // Joypad

    // Clear IF bit
    memory_write8(0xFF0F, iflag & ~bit);

    // Push PC
    cpu.sp--;
    memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
    cpu.sp--;
    memory_write8(cpu.sp, cpu.pc & 0xFF);

    cpu.pc = vector;

    return 20; // interrupt cost
}

int cpu_step() {
    // Check interrupts  
    
    int cycles = handle_interrupts();
    if (cycles > 0) return cycles;    

    if (cpu.halted) return 4;

    uint8_t opcode = fetch8();

    switch (opcode) {
        case 0x00: // NOP
            cycles = 4;
            break;
        
        case 0x01: { // LD BC,u16
            uint16_t value = fetch16();
            cpu.bc = value;
            cycles = 12;
            break;
        }

        case 0x02: // LD (BC), A
            memory_write8(cpu.bc, cpu.a);
            cycles = 8;
            break;

        case 0x03: // INC BC
            cpu.bc++;
            cycles = 8;
            break;

        case 0x04: // INC B
            cycles = inc(&cpu.b);
            break;

        case 0x05: // DEC B
            cycles = dec(&cpu.b);
            break;

        case 0x06: { // LD B, n
            uint8_t value = fetch8();
            cpu.b = value;
            cycles = 8;
            break;
        }

        case 0x07: // RLCA
            cycles = rlca(&cpu.a);
            break;

        case 0x08: { // LD (u16),SP
            uint16_t addr = fetch16();
            memory_write8(addr, cpu.sp & 0xFF); // low byte
            memory_write8(addr + 1, (cpu.sp >> 8) & 0xFF); // high byte
            cycles = 20;
            break;
        }

        case 0x09: // ADD HL,BC
            cycles = add_16(&cpu.hl, cpu.bc);
            break;

        case 0x0A: // LD A,(BC)
            cpu.a = memory_read8(cpu.bc);
            cycles = 8;
            break;

        case 0x0B: // DEC BC
            cpu.bc--;
            cycles = 8;
            break;

        case 0x0C: // INC C
            cycles = inc(&cpu.c);
            break;

        case 0x0D: // DEC C
            cycles = dec(&cpu.c);
            break;

        case 0x0E: // LD C, n
            cpu.c = fetch8();
            cycles = 8;
            break;

        case 0x0F: // RRCA
            cycles = rrca(&cpu.a);
            break;

        case 0x10: // STOP (not quite right I guess)
            cpu.pc++;
            return 4;

        case 0x11: { // LD DE,u16
            uint16_t value = fetch16();
            cpu.de = value;
            cycles = 12;
            break;
        }

        case 0x12: // LD (DE),A
            memory_write8(cpu.de, cpu.a);
            cycles = 8;
            break;

        case 0x13: // INC DE
            cpu.de++;
            cycles = 8;
            break;

        case 0x14: // INC D
            cycles = inc(&cpu.d);
            break;

        case 0x15: // DEC D
            cycles = dec(&cpu.d);
            break;

        case 0x16: { // LD D,u8
            uint8_t value = fetch8();
            cpu.d = value;
            cycles = 8;
            break;
        }

        case 0x17: // RLA
            cycles = rla(&cpu.a);
            break;

        case 0x18: { // JR i8
            uint8_t offset = memory_read8(cpu.pc++); // it is just fetch8()
            cpu.pc += (int8_t)offset;
            cycles = 12;
            break;
        }

        case 0x19: // ADD HL,DE
            cycles = add_16(&cpu.hl, cpu.de);
            break;

        case 0x1A: // LD A,(DE)
            cpu.a = memory_read8(cpu.de);
            cycles = 8;
            break;

        case 0x1B: // DEC DE
            cpu.de--;
            cycles = 8;
            break;

        case 0x1C: // INC E
            cycles = inc(&cpu.e);
            break;

        case 0x1D: // DEC E
            cycles = dec(&cpu.e);
            break;

        case 0x1E: { // LD E, n
            uint8_t value = fetch8();
            cpu.e = value;
            cycles = 8;
            break;
        }

        case 0x1F: // RRA
            cycles = rra(&cpu.a);
            break;

        case 0x20: { // JR NZ, i8
            int8_t offset = (int8_t)fetch8();
            if (!CHECK_FLAG(FLAG_Z)) {
                cpu.pc += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x21: { // LD HL,u16
            uint16_t value = fetch16();
            cpu.hl = value;
            cycles = 12;
            break;
        }

        case 0x22: // LD (HL+),A
            memory_write8(cpu.hl, cpu.a);
            cpu.hl++;
            cycles = 8;
            break;

        case 0x23: // INC HL
            cpu.hl++;
            cycles = 8;
            break;

        case 0x24: // INC H
            cycles = inc(&cpu.h);
            break;

        case 0x25: // DEC H
            cycles = dec(&cpu.h);
            break;

        case 0x26: // LD H, u8
            cpu.h = fetch8();
            cycles = 8;
            break;

        case 0x27: // DAA
            cycles = daa(&cpu.a);
            break;

        case 0x28: { // JR Z, i8
            int8_t offset = (int8_t)fetch8();
            if (CHECK_FLAG(FLAG_Z)) {
                cpu.pc += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x29: // ADD HL,HL
            cycles = add_16(&cpu.hl, cpu.hl);
            break;

        case 0x2A: // LD A,(HL+)
            cpu.a = memory_read8(cpu.hl);
            cpu.hl++;
            cycles = 8;
            break;

        case 0x2B: // DEC HL
            cpu.hl--;
            cycles = 8;
            break;

        case 0x2C: // INC L
            cycles = inc(&cpu.l);
            break;

        case 0x2D: // DEC L
            cycles = dec(&cpu.l);
            break;

        case 0x2E: // LD L, u8
            cpu.l = fetch8();
            cycles = 8;
            break;

        case 0x2F: // CPL
            cpu.a = ~cpu.a;
            SET_FLAG(FLAG_N | FLAG_H);
            cycles = 4;
            break;

        case 0x30: { // JR NC, i8
            int8_t offset = (int8_t)fetch8();
            if (!CHECK_FLAG(FLAG_C)) {
                cpu.pc += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x31: { // LD SP,u16
            uint16_t value = fetch16();
            cpu.sp = value;
            cycles = 12;
            break;
        }

        case 0x32: // LD (HL-),A
            memory_write8(cpu.hl, cpu.a);
            cpu.hl--;
            cycles = 8;
            break;

        case 0x33: // INC SP
            cpu.sp++;
            cycles = 8;
            break;

        case 0x34: { // INC (HL)
            uint8_t value = memory_read8(cpu.hl);
            inc(&value);
            memory_write8(cpu.hl, value);
            cycles = 12;
            break;
        }

        case 0x35: { // DEC (HL)
            uint8_t value = memory_read8(cpu.hl);
            dec(&value);
            memory_write8(cpu.hl, value);
            cycles = 12;
            break;
        }

        case 0x36: { // LD (HL),u8
            uint8_t value = fetch8();
            memory_write8(cpu.hl, value);
            cycles = 12;
            break;
        }

        case 0x37 : // SCF
            SET_FLAG(FLAG_C);
            CLEAR_FLAG(FLAG_N | FLAG_H);
            cycles = 4;
            break;

        case 0x38: { // JR C,i8 - 0x38
            int8_t offset = (int8_t)fetch8();

            if(CHECK_FLAG(FLAG_C)) {
                cpu.pc += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x39: // ADD HL,SP
            cycles = add_16(&cpu.hl, cpu.sp);
            break;

        case 0x3A: // LD A,(HL-)
            cpu.a = memory_read8(cpu.hl);
            cpu.hl--;
            cycles = 8;
            break;

        case 0x3B: // DEC SP
            cpu.sp--;
            cycles = 8;
            break;

        case 0x3C: // INC A
            cycles = inc(&cpu.a);
            break;

        case 0x3D: // DEC A
            cycles = dec(&cpu.a);
            break;

        case 0x3E: // LD A,u8
            cpu.a = fetch8();
            cycles = 8;
            break;

        case 0x3F: // CCF
            WRITE_FLAG(FLAG_C, !CHECK_FLAG(FLAG_C));
            CLEAR_FLAG(FLAG_N | FLAG_H);
            cycles = 4;
            break;

        case 0x40: // LD B,B
            cpu.b = cpu.b;
            cycles = 4;
            break;

        case 0x41: // LD B,C
            cpu.b = cpu.c;
            cycles = 4;
            break;

        case 0x42: // LD B,D
            cpu.b = cpu.d;
            cycles = 4;
            break;

        case 0x43: // LD B,E
            cpu.b = cpu.e;
            cycles = 4;
            break;

        case 0x44: // LD B,H
            cpu.b = cpu.h;
            cycles = 4;
            break;

        case 0x45: // LD B,L
            cpu.b = cpu.l;
            cycles = 4;
            break;

        case 0x46: // LD B,(HL)
            cpu.b = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x47: // LD B,A
            cpu.b = cpu.a;
            cycles = 4;
            break;

        case 0x48: // LD C,B
            cpu.c = cpu.b;
            cycles = 4;
            break;

        case 0x49: // LD C,C
            cpu.c = cpu.c;
            cycles = 4;
            break;

        case 0x4A: // LD C,D
            cpu.c = cpu.d;
            cycles = 4;
            break;

        case 0x4B: // LD C,E
            cpu.c = cpu.e;
            cycles = 4;
            break;

        case 0x4C: // LD C,H
            cpu.c = cpu.h;
            cycles = 4;
            break;

        case 0x4D: // LD C,L
            cpu.c = cpu.l;
            cycles = 4;
            break;

        case 0x4E: // LD C,(HL)
            cpu.c = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x4F: // LD C,A
            cpu.c = cpu.a;
            cycles = 4;
            break;

        case 0x50: // LD D,B
            cpu.d = cpu.b;
            cycles = 4;
            break;

        case 0x51: // LD D,C
            cpu.d = cpu.c;
            cycles = 4;
            break;

        case 0x52: // LD D,D
            cpu.d = cpu.d;
            cycles = 4;
            break;

        case 0x53: // LD D,E
            cpu.d = cpu.e;
            cycles = 4;
            break;

        case 0x54: // LD D,H
            cpu.d = cpu.h;
            cycles = 4;
            break;

        case 0x55: // LD D,L
            cpu.d = cpu.l;
            cycles = 4;
            break;

        case 0x56: // LD D,(HL)
            cpu.d = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x57: // LD D,A
            cpu.d = cpu.a;
            cycles = 4;
            break;

        case 0x58: // LD E,B
            cpu.e = cpu.b;
            cycles = 4;
            break;

        case 0x59: // LD E,C
            cpu.e = cpu.c;
            cycles = 4;
            break;

        case 0x5A: // LD E,D
            cpu.e = cpu.d;
            cycles = 4;
            break;

        case 0x5B: // LD E,E
            cpu.e = cpu.e;
            cycles = 4;
            break;

        case 0x5C: // LD E,H
            cpu.e = cpu.h;
            cycles = 4;
            break;

        case 0x5D: // LD E,L
            cpu.e = cpu.l;
            cycles = 4;
            break;

        case 0x5E: // LD E,(HL)
            cpu.e = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x5F: // LD E,A
            cpu.e = cpu.a;
            cycles = 4;
            break;

        case 0x60: // LD H,B
            cpu.h = cpu.b;
            cycles = 4;
            break;

        case 0x61: // LD H,C
            cpu.h = cpu.c;
            cycles = 4;
            break;

        case 0x62: // LD H,D
            cpu.h = cpu.d;
            cycles = 4;
            break;

        case 0x63: // LD H,E
            cpu.h = cpu.e;
            cycles = 4;
            break;

        case 0x64: //LD H,H
            cpu.h = cpu.h;
            cycles = 4;
            break;

        case 0x65: // LD H,L
            cpu.h = cpu.l;
            cycles = 4;
            break;

        case 0x66: // LD H,(HL)
            cpu.h = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x67: // LD H,A
            cpu.h = cpu.a;
            cycles = 4;
            break;

        case 0x68: // LD L,B
            cpu.l = cpu.b;
            cycles = 4;
            break;

        case 0x69: //LD L,C
            cpu.l = cpu.c;
            cycles = 4;
            break;

        case 0x6A: // LD L,D
            cpu.l = cpu.d;
            cycles = 4;
            break;

        case 0x6B: // LD L,E
            cpu.l = cpu.e;
            cycles = 4;
            break;

        case 0x6C: // LD L,H
            cpu.l = cpu.h;
            cycles = 4;
            break;

        case 0x6D: // LD L,L
            cpu.l = cpu.l;
            cycles = 4;
            break;

        case 0x6E: // LD L,(HL)
            cpu.l = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x6F: // LD L,A
            cpu.l = cpu.a;
            cycles = 4;
            break;

        case 0x70: // LD (HL),B
            memory_write8(cpu.hl, cpu.b);
            cycles = 8;
            break;

        case 0x71: // LD (HL),C
            memory_write8(cpu.hl, cpu.c);
            cycles = 8;
            break;

        case 0x72: // LD (HL),D
            memory_write8(cpu.hl, cpu.d);
            cycles = 8;
            break;

        case 0x73: // LD (HL),E
            memory_write8(cpu.hl, cpu.e);
            cycles = 8;
            break;

        case 0x74: // LD (HL),H
            memory_write8(cpu.hl, cpu.h);
            cycles = 8;
            break;

        case 0x75: // LD (HL),L
            memory_write8(cpu.hl, cpu.l);
            cycles = 8;
            break;

        case 0x76:  // HALT
            cpu.halted = 1;
            cycles = 4;
            break;

        case 0x77: // LD (HL), A
            memory_write8(cpu.hl, cpu.a);
            cycles = 8;
            break;

        case 0x78: // LD A,B
            cpu.a = cpu.b;
            cycles = 4;
            break;
        
        case 0x79: // LD A,C
            cpu.a = cpu.c;
            cycles = 4;
            break;

        case 0x7A: // LD A,D
            cpu.a = cpu.d;
            cycles = 4;
            break;

        case 0x7B: // LD A,E
            cpu.a = cpu.e;
            cycles = 4;
            break;

        case 0x7C: // LD A,H
            cpu.a = cpu.h;
            cycles = 4;
            break;

        case 0x7D: // LD A,L
            cpu.a = cpu.l;
            cycles = 4;
            break;

        case 0x7E: // LD A,(HL)
            cpu.a = memory_read8(cpu.hl);
            cycles = 8;
            break;

        case 0x7F: // LD A,A
            cpu.a = cpu.a;
            cycles = 4;
            break;

        case 0x80: // ADD A,B
            cycles = add(&cpu.a, cpu.b);
            break;

        case 0x81: // ADD A,C
            cycles = add(&cpu.a, cpu.c);
            break;

        case 0x82: // ADD A,D
            cycles = add(&cpu.a, cpu.d);
            break;

        case 0x83: // ADD A,E
            cycles = add(&cpu.a, cpu.e);
            break;

        case 0x84: // ADD A,H
            cycles = add(&cpu.a, cpu.h);
            break;

        case 0x85: // ADD A,L
            cycles = add(&cpu.a, cpu.l);
            break;

        case 0x86: { // ADD A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            add(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0x87: // ADD A,A
            cycles = add(&cpu.a, cpu.a);
            break;

        case 0x88: // ADC A,B
            cycles = adc(&cpu.a, cpu.b);
            break;

        case 0x89: // ADC A,C
            cycles = adc(&cpu.a, cpu.c);
            break;

        case 0x8A: // ADC A,D
            cycles = adc(&cpu.a, cpu.d);
            break;

        case 0x8B: // ADC A,E
            cycles = adc(&cpu.a, cpu.e);
            break;

        case 0x8C: // ADC A,H
            cycles = adc(&cpu.a, cpu.h);
            break;

        case 0x8D: // ADC A,L
            cycles = adc(&cpu.a, cpu.l);
            break;

        case 0x8E: { // ADC A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            adc(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0x8F: // ADC A,A
            cycles = adc(&cpu.a, cpu.a);
            break;

        case 0x90: // SUB A,B
            cycles = sub(&cpu.a, cpu.b);
            break;

        case 0x91: // SUB A,C
            cycles = sub(&cpu.a, cpu.c);
            break;

        case 0x92: // SUB A,D
            cycles = sub(&cpu.a, cpu.d);
            break;

        case 0x93: // SUB A,E
            cycles = sub(&cpu.a, cpu.e);
            break;

        case 0x94: // SUB A,H
            cycles = sub(&cpu.a, cpu.h);
            break;

        case 0x95: // SUB A,L
            cycles = sub(&cpu.a, cpu.l);
            break;

        case 0x96: { // SUB A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            sub(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0x97: // SUB A,A
            cycles = sub(&cpu.a, cpu.a);
            break;

        case 0x98: // SBC A,B
            cycles = sbc(&cpu.a, cpu.b);
            break;

        case 0x99: // SBC A,C
            cycles = sbc(&cpu.a, cpu.c);
            break;

        case 0x9A: // SBC A,D
            cycles = sbc(&cpu.a, cpu.d);
            break;

        case 0x9B: // SBC A,E
            cycles = sbc(&cpu.a, cpu.e);
            break;

        case 0x9C: // SBC A,H
            cycles = sbc(&cpu.a, cpu.h);
            break;

        case 0x9D: // SBC A,L
            cycles = sbc(&cpu.a, cpu.l);
            break;

        case 0x9E: { // SBC A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            sbc(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0x9F: // SBC A,A
            cycles = sbc(&cpu.a, cpu.a);
            break;

        case 0xA0: // AND A,B
            cycles = and_(&cpu.a, cpu.b);
            break;

        case 0xA1: // AND A,C
            cycles = and_(&cpu.a, cpu.c);
            break;

        case 0xA2: // AND A,D
            cycles = and_(&cpu.a, cpu.d);
            break;

        case 0xA3: // AND A,E
            cycles = and_(&cpu.a, cpu.e);
            break;

        case 0xA4: // AND A,H
            cycles = and_(&cpu.a, cpu.h);
            break;

        case 0xA5: // AND A,L
            cycles = and_(&cpu.a, cpu.l);
            break;

        case 0xA6: { // AND A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            and_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xA7: // AND A,A
            cycles = and_(&cpu.a, cpu.a);
            break;

        case 0xA8: // XOR A,B
            cycles = xor_(&cpu.a, cpu.b);
            break;

        case 0xA9: // XOR A,C
            cycles = xor_(&cpu.a, cpu.c);
            break;

        case 0xAA: // XOR A,D
            cycles = xor_(&cpu.a, cpu.d);
            break;

        case 0xAB: // XOR A,E
            cycles = xor_(&cpu.a, cpu.e);
            break;

        case 0xAC: // XOR A,H
            cycles = xor_(&cpu.a, cpu.h);
            break;

        case 0xAD: // XOR A,L
            cycles = xor_(&cpu.a, cpu.l);
            break;

        case 0xAE: { // XOR A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            xor_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xAF: // XOR A,A
            cycles = xor_(&cpu.a, cpu.a);
            break;

        case 0xB0: // OR A,B
            cycles = or_(&cpu.a, cpu.b);
            break;

        case 0xB1: // OR A,C
            cycles = or_(&cpu.a, cpu.c);
            break;

        case 0xB2: // OR A,D
            cycles = or_(&cpu.a, cpu.d);
            break;

        case 0xB3: // OR A,E
            cycles = or_(&cpu.a, cpu.e);
            break;

        case 0xB4: // OR A,H
            cycles = or_(&cpu.a, cpu.h);
            break;

        case 0xB5: // OR A,L
            cycles = or_(&cpu.a, cpu.l);
            break;

        case 0xB6: { // OR A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            or_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xB7: // OR A,A
            cycles = or_(&cpu.a, cpu.a);
            break;

        case 0xB8: // CP A,B
            cycles = cp(cpu.a, cpu.b);
            break;

        case 0xB9: // CP A,C
            cycles = cp(cpu.a, cpu.c);
            break;

        case 0xBA: // CP A,D
            cycles = cp(cpu.a, cpu.d);
            break;

        case 0xBB: // CP A,E
            cycles = cp(cpu.a, cpu.e);
            break;

        case 0xBC: // CP A,H
            cycles = cp(cpu.a, cpu.h);
            break;

        case 0xBD: // CP A,L
            cycles = cp(cpu.a, cpu.l);
            break;

        case 0xBE: { // CP A,(HL)
            uint8_t value = memory_read8(cpu.hl);
            cp(cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xBF: // CP A,A
            cycles = cp(cpu.a, cpu.a);
            break;

        case 0xC0: { // RET NZ 
            if(!CHECK_FLAG(FLAG_Z)) {
                cpu.pc = memory_read8(cpu.sp) | (memory_read8(cpu.sp + 1) << 8);
                cpu.sp += 2;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0xC1: { // POP BC
            uint8_t lsb = memory_read8(cpu.sp);
            cpu.sp++;
            uint8_t msb = memory_read8(cpu.sp);
            cpu.sp++;
            cpu.bc = lsb | (msb << 8);
            cycles = 12;
            break;
        }

        case 0xC2: { // JP NZ, nn
            uint16_t nn = fetch16();
            if (!CHECK_FLAG(FLAG_Z)) {
                cpu.pc = nn;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xC3: { // JP nn
            uint16_t addr = fetch16();
            cpu.pc = addr;
            cycles = 16;
            break;
        }

        case 0xC4: { // CALL NZ, u16
            uint16_t nn = fetch16();
            if (!CHECK_FLAG(FLAG_Z)) {
                cpu.sp--;
                memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
                cpu.sp--;
                memory_write8(cpu.sp, cpu.pc & 0xFF);
                cpu.pc = nn;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xC5: // PUSH BC
            cpu.sp -= 2;
            memory_write8(cpu.sp, cpu.bc & 0xFF);
            memory_write8(cpu.sp+1, (cpu.bc >> 8) & 0xFF);
            cycles = 16;
            break;

        case 0xC6: { // ADD A,u8
            uint8_t value = fetch8();
            add(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xC7: // RST 00h
            cycles = rst(0x0000);
            break;

        case 0xC8: { // RET Z
            if (CHECK_FLAG(FLAG_Z)) {
                uint16_t value = memory_read8(cpu.sp) | (memory_read8(cpu.sp+1) << 8);
                cpu.sp += 2;
                cpu.pc = value;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0xC9: { // RET
            uint8_t lsb = memory_read8(cpu.sp);
            cpu.sp++;
            uint8_t msb = memory_read8(cpu.sp);
            cpu.sp++;
            cpu.pc = lsb | (msb << 8);
            cycles = 16;
            break;
        }

        case 0xCA: {  // JP Z,u16
            uint16_t dest = fetch16();
            if (CHECK_FLAG(FLAG_Z)) {
                cpu.pc = dest;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xCB: { // PREFIX - CB
            uint8_t opcode_cb = fetch8();
            cycles = execute_cb_instruction(opcode_cb);
            break;
        }

        case 0xCC: { // CALL Z, u16
            uint16_t nn = fetch16();
            if (CHECK_FLAG(FLAG_Z)) {
                cpu.sp--;
                memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
                cpu.sp--;
                memory_write8(cpu.sp, cpu.pc & 0xFF);
                cpu.pc = nn;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xCD: { // CALL u16
            uint8_t nn_lsb = memory_read8(cpu.pc);
            cpu.pc++;
            uint8_t nn_msb = memory_read8(cpu.pc);
            cpu.pc++;
            uint16_t nn = nn_lsb | (nn_msb << 8);
            cpu.sp--;
            memory_write8(cpu.sp, cpu.pc >> 8);
            cpu.sp--;
            memory_write8(cpu.sp, cpu.pc & 0xFF);
            cpu.pc = nn;
            cycles = 24;
            break;
        }

        case 0xCE: { // ADC A,u8
            uint8_t value = fetch8();
            adc(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xCF: // RST 08h
            cycles = rst(0x0008);
            break;  

        case 0xE0: { // LD (0xFF00 + u8), A
            uint16_t address = 0xFF00 + fetch8();
            memory_write8(address, cpu.a);
            cycles = 12;
            break;
        }

        case 0xE1: { // POP HL
            uint8_t lsb = memory_read8(cpu.sp);
            cpu.sp++;
            uint8_t msb = memory_read8(cpu.sp);
            cpu.sp++;
            cpu.hl = lsb | (msb << 8);
            cycles = 12;
            break;
        }

        case 0xE2: { // LD (FF00+C),A
            uint16_t address = 0xFF00 + cpu.c;
            memory_write8(address, cpu.a);
            cycles = 8;
            break;
        }

        case 0xE5: { // PUSH HL
            cpu.sp -= 2;
            memory_write8(cpu.sp, cpu.hl & 0xFF);
            memory_write8(cpu.sp+1, (cpu.hl >> 8) & 0xFF);
            cycles = 16;
            break;
        }

        case 0xE6: { // AND A,u8
            uint8_t value = fetch8();
            and_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xE7: // RST 20h
            cycles = rst(0x0020);
            break;

        case 0xE8: { // ADD SP,i8
            int8_t value = (int8_t)fetch8();
            uint16_t result = cpu.sp + value;

            WRITE_FLAG(FLAG_H, ((cpu.sp & 0xF) + (value & 0xF)) > 0xF);
            WRITE_FLAG(FLAG_C, ((cpu.sp & 0xFF) + (value & 0xFF)) > 0xFF);
            CLEAR_FLAG(FLAG_Z | FLAG_N);

            cpu.sp = result;

            cycles = 16;
            break;
        }

        case 0xE9: // JP HL
            cpu.pc = cpu.hl;
            cycles = 4;
            break;

        case 0xEA: { // LD (u16),A
            uint16_t address = fetch16();
            memory_write8(address, cpu.a);
            cycles = 16;
            break;
        }

        case 0xEE: { // XOR A, u8
            uint8_t value = fetch8();
            xor_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xEF: // RST 28h
            cycles = rst(0x0028);
            break;

        case 0xD0: { // RET NC 
            if(!CHECK_FLAG(FLAG_C)) {
                cpu.pc = memory_read8(cpu.sp) | (memory_read8(cpu.sp + 1) << 8);
                cpu.sp += 2;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0xD1: { // POP DE
            uint8_t lsb = memory_read8(cpu.sp);
            cpu.sp++;
            uint8_t msb = memory_read8(cpu.sp);
            cpu.sp++;
            cpu.de = lsb | (msb << 8);
            cycles = 12;
            break;
        }

        case 0xD2: { // JP NC, nn
            uint16_t nn = fetch16();
            if (!CHECK_FLAG(FLAG_C)) {
                cpu.pc = nn;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xD4: { // CALL NC, u16
            uint16_t nn = fetch16();
            if (!CHECK_FLAG(FLAG_C)) {
                cpu.sp--;
                memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
                cpu.sp--;
                memory_write8(cpu.sp, cpu.pc & 0xFF);
                cpu.pc = nn;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }


        case 0xD5: // PUSH DE
            cpu.sp -= 2;
            memory_write8(cpu.sp, cpu.de & 0xFF);
            memory_write8(cpu.sp+1, (cpu.de >> 8) & 0xFF);
            cycles = 16;
            break;

        case 0xD6: { // SUB A,u8
            uint8_t value = fetch8();
            sub(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xD7: // RST 10h
            cycles = rst(0x0010);
            break;

        case 0xD8: { // RET C
            if (CHECK_FLAG(FLAG_C)) {
                uint16_t value = memory_read8(cpu.sp) | (memory_read8(cpu.sp+1) << 8);
                cpu.sp += 2;
                cpu.pc = value;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0xD9: { // RETI
            cpu.ime = 1;
            cpu.pc = memory_read8(cpu.sp) | (memory_read8(cpu.sp + 1) << 8);
            cpu.sp += 2;
            cycles = 16;
            break;
        }

        case 0xDA: { // JP C,u16
            uint16_t nn = fetch16();
            if (CHECK_FLAG(FLAG_C)) {
                cpu.pc = nn;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xDB: {
            break;
        }

        case 0xDC: { // CALL C, u16
            uint16_t nn = fetch16();
            if (CHECK_FLAG(FLAG_C)) {
                cpu.sp--;
                memory_write8(cpu.sp, (cpu.pc >> 8) & 0xFF);
                cpu.sp--;
                memory_write8(cpu.sp, cpu.pc & 0xFF);
                cpu.pc = nn;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xDE: { // SBC A,u8
            uint8_t value = fetch8();
            sbc(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xDF: // RST 18h
            cycles = rst(0x0018);
            break;

        case 0xF0: { // LD A,(FF00+u8)
            uint16_t address = 0xFF00 + fetch8();
            cpu.a = memory_read8(address);
            cycles = 12;
            break;
        }

        case 0xF1: { // POP AF
            uint8_t lsb = memory_read8(cpu.sp);
            cpu.sp++;
            uint8_t msb = memory_read8(cpu.sp);
            cpu.sp++;
            cpu.af = lsb | (msb << 8);
            cpu.f &= 0xf0;
            cycles = 12;
            break;
        }

        case 0xF2: { // LD A,(FF00+C)
            uint16_t address = 0xFF00 + cpu.c;
            cpu.a = memory_read8(address);
            cycles = 8;
            break;
        }

        case 0xF3: // DI
            cpu.ime = 0;
            cpu.ime_pending = 0;
            cycles = 4;
            break;

        case 0xF5: // PUSH AF
            cpu.sp -= 2;
            memory_write8(cpu.sp, cpu.af & 0xFF);
            memory_write8(cpu.sp+1, (cpu.af >> 8) & 0xFF);
            cycles = 16;
            break;

        case 0xF6: { // OR A,u8
            uint8_t value = fetch8();
            or_(&cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xF7: // RST 30h
            cycles = rst(0x0030);
            break;

        case 0xF8: { // LD HL,SP+i8
            int8_t value = (int8_t)fetch8();
            uint16_t result = cpu.sp + value;

            cpu.hl = result;

            WRITE_FLAG(FLAG_H, ((cpu.sp & 0xF) + (value & 0xF)) > 0xF);
            WRITE_FLAG(FLAG_C, ((cpu.sp & 0xFF) + (value & 0xFF)) > 0xFF);
            CLEAR_FLAG(FLAG_Z | FLAG_N);
            
            cycles = 12;
            break;
        }

        case 0xF9: // LD SP,HL
            cpu.sp = cpu.hl;
            cycles = 8;
            break;

        case 0xFA: { // LD A,(u16) // LD A, (nn)
            uint8_t nn_lsb = memory_read8(cpu.pc);
            cpu.pc++;
            uint8_t nn_msb = memory_read8(cpu.pc);
            cpu.pc++;
            uint16_t nn = nn_lsb | (nn_msb << 8);
            cpu.a = memory_read8(nn);
            cycles = 16;
            break;
        }

        case 0xFB: // EI
            cpu.ime_pending = 2;
            cycles = 4;
            break;

        case 0xFE: { // CP A, u8
            uint8_t value = fetch8();
            cp(cpu.a, value);
            cycles = 8;
            break;
        }

        case 0xFF: // RST 38h
            cycles = rst(0x0038);
            break;

        default:
            printf("Unimplemented opcode: 0x%02X at 0x%04X\n", opcode, cpu.pc - 1);
            cpu.halted = 1;
            cycles = 0;
            break;
    }

    if (cpu.ime_pending > 0) {
        cpu.ime_pending--;
        if (cpu.ime_pending == 0) {
            cpu.ime = 1;
        }
    }

    return cycles;
}

int execute_cb_instruction(uint8_t opcode_cb) {
    switch (opcode_cb) {
        case 0x00: // RLC B
            return rlc(&cpu.b);

        case 0x01: // RLC C
            return rlc(&cpu.c);

        case 0x02: // RLC D
            return rlc(&cpu.d);

        case 0x03: // RLC E
            return rlc(&cpu.e);

        case 0x04: // RLC H
            return rlc(&cpu.h);

        case 0x05: // RLC L
            return rlc(&cpu.l);

        case 0x06: { // RLC (HL)
            uint8_t value = memory_read8(cpu.hl);
            rlc(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x07: // RLC A
            return rlc(&cpu.a);

        case 0x08: // RRC B
            return rrc(&cpu.b);

        case 0x09: // RRC C
            return rrc(&cpu.c);
        
        case 0x0A: // RRC D
            return rrc(&cpu.d);

        case 0x0B: // RRC E
            return rrc(&cpu.e);

        case 0x0C: // RRC H
            return rrc(&cpu.h);

        case 0x0D: // RRC L
            return rrc(&cpu.l);

        case 0x0E: { // RRC (HL)
            uint8_t value = memory_read8(cpu.hl);
            rrc(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x0F: // RRC A
            return rrc(&cpu.a);

        case 0x10: // RL B
            return rl(&cpu.b);        

        case 0x11: // RL C
            return rl(&cpu.c);

        case 0x12: // RL D
            return rl(&cpu.d);

        case 0x13: // RL E
            return rl(&cpu.e);

        case 0x14: // RL H
            return rl(&cpu.h);

        case 0x15: // RL L
            return rl(&cpu.l);

        case 0x16: {  // RL (HL)
            uint8_t value = memory_read8(cpu.hl);
            rl(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x17: // RL A
            return rl(&cpu.a);

        case 0x18: // RR B
            return rr(&cpu.b);

        case 0x19: // RR C
            return rr(&cpu.c);

        case 0x1A: // RR D
            return rr(&cpu.d);

        case 0x1B: // RR E
            return rr(&cpu.e);

        case 0x1C: // RR H
            return rr(&cpu.h);

        case 0x1D: // RR L
            return rr(&cpu.l);

        case 0x1E: { // RR (HL)
            uint8_t value = memory_read8(cpu.hl);
            rr(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x1F: // RR A
            return rr(&cpu.a);

        case 0x20: // SLA B
            return sla(&cpu.b);

        case 0x21: // SLA C
            return sla(&cpu.c);

        case 0x22: // SLA D
            return sla(&cpu.d);

        case 0x23: // SLA E
            return sla(&cpu.e);

        case 0x24: // SLA H
            return sla(&cpu.h);

        case 0x25: // SLA L
            return sla(&cpu.l);

        case 0x26: { // SLA (HL)
            uint8_t value = memory_read8(cpu.hl);
            sla(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x27: // SLA A
            return sla(&cpu.a);

        case 0x28: // SRA B
            return sra(&cpu.b);

        case 0x29: // SRA C
            return sra(&cpu.c);

        case 0x2A: // SRA D
            return sra(&cpu.d);

        case 0x2B: // SRA E
            return sra(&cpu.e);

        case 0x2C: // SRA H
            return sra(&cpu.h);

        case 0x2D: // SRA L
            return sra(&cpu.l);

        case 0x2E: { // SRA (HL)
            uint8_t value = memory_read8(cpu.hl);
            sra(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x2F: // SRA A
            return sra(&cpu.a);

        case 0x30: // SWAP B
            return swap(&cpu.b);

        case 0x31: // SWAP C
            return swap(&cpu.c);

        case 0x32: // SWAP D
            return swap(&cpu.d);

        case 0x33: // SWAP E
            return swap(&cpu.e);

        case 0x34: // SWAP H
            return swap(&cpu.h);

        case 0x35: // SWAP L
            return swap(&cpu.l);

        case 0X36: { // SWAP (HL)
            uint8_t value = memory_read8(cpu.hl);
            swap(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x37: // SWAP A
            return swap(&cpu.a);

        case 0x38: // SRL B
            return srl(&cpu.b);

        case 0x39: // SRL C
            return srl(&cpu.c);

        case 0x3A: // SRL D
            return srl(&cpu.d);

        case 0x3B: // SRL E
            return srl(&cpu.e);

        case 0x3C: // SRL H
            return srl(&cpu.h);

        case 0x3D: // SRL L
            return srl(&cpu.l);

        case 0x3E: { // SRL (HL)
            uint8_t value = memory_read8(cpu.hl);
            srl(&value);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x3F: // SRL A
            return srl(&cpu.a);

        case 0x40: // BIT 0,B
            return bit(cpu.b, 0);

        case 0x41: // BIT 0,C
            return bit(cpu.c, 0);

        case 0x42: // BIT 0,D
            return bit(cpu.d, 0);

        case 0x43: // BIT 0,E
            return bit(cpu.e, 0);

        case 0x44: // BIT 0,H
            return bit(cpu.h, 0);
        
        case 0x45: // BIT 0,L
            return bit(cpu.l, 0);

        case 0x46: { // BIT 0,(HL)
            bit(memory_read8(cpu.hl), 0);
            return 12;
        }

        case 0x47: // BIT 0,A
            return bit(cpu.a, 0);

        case 0x48: // BIT 1,B
            return bit(cpu.b, 1);

        case 0x49: // BIT 1,C
            return bit(cpu.c, 1);

        case 0x4A: // BIT 1,D
            return bit(cpu.d, 1);

        case 0x4B: // BIT 1,E
            return bit(cpu.e, 1);

        case 0x4C: // BIT 1,H
            return bit(cpu.h, 1);

        case 0x4D: // BIT 1,L
            return bit(cpu.l, 1);

        case 0x4E: { // BIT 1,(HL)
            bit(memory_read8(cpu.hl), 1);
            return 12;
        }

        case 0x4F: // BIT 1,A
            return bit(cpu.a, 1);

        case 0x50: // BIT 2,B
            return bit(cpu.b, 2);

        case 0x51: // BIT 2,C
            return bit(cpu.c, 2);

        case 0x52: // BIT  2,D
            return bit(cpu.d, 2);

        case 0x53: // BIT 2,E
            return bit(cpu.e, 2);

        case 0x54: // BIT 2,H
            return bit(cpu.h, 2);

        case 0x55: // BIT 2,L
            return bit(cpu.l, 2);

        case 0x56: { // BIT 2,(HL)
            bit(memory_read8(cpu.hl), 2);
            return 12;
        }

        case 0x57: // BIT 2,A
            return bit(cpu.a, 2);

        case 0x58: // BIT 3,B
            return bit(cpu.b, 3);
        
        case 0x59: // BIT 3,C
            return bit(cpu.c, 3);

        case 0x5A: // BIT 3,D
            return bit(cpu.d, 3);

        case 0x5B: // BIT 3,E
            return bit(cpu.e, 3);

        case 0x5C: // BIT 3,H
            return bit(cpu.h, 3);

        case 0x5D: // BIT 3,L
            return bit(cpu.l, 3);

        case 0x5E: { // BIT 3,(HL)
            bit(memory_read8(cpu.hl), 3);
            return 12;
        }

        case 0x5F: // BIT 3,A
            return bit(cpu.a, 3);

        case 0x60: // BIT 4,B
            return bit(cpu.b, 4);

        case 0x61: // BIT 4,C
            return bit(cpu.c, 4);

        case 0x62: // BIT 4,D
            return bit(cpu.d, 4);

        case 0x63: // BIT 4,E
            return bit(cpu.e, 4);

        case 0x64: // BIT 4,H
            return bit(cpu.h, 4);

        case 0x65: // BIT 4,L
            return bit(cpu.l, 4);

        case 0x66: { // BIT 4,(HL)
            bit(memory_read8(cpu.hl), 4);
            return 12;
        }

        case 0x67: // BIT 4,A
            return bit(cpu.a, 4);

        case 0x68: // BIT 5,B
            return bit(cpu.b, 5);

        case 0x69: // BIT 5,C
            return bit(cpu.c, 5);

        case 0x6A: // BIT 5,D
            return bit(cpu.d, 5);

        case 0x6B: // BIT 5,E
            return bit(cpu.e, 5);

        case 0x6C: // BIT 5,H
            return bit(cpu.h, 5);

        case 0x6D: // BIT 5,L
            return bit(cpu.l, 5);

        case 0x6E: { // BIT 5,(HL)
            bit(memory_read8(cpu.hl), 5);
            return 12;
        }

        case 0x6F: { // BIT 5,A
            return bit(cpu.a, 5);
        }

        case 0x70: // BIT 6,B
            return bit(cpu.b, 6);

        case 0x71: // BIT 6,C
            return bit(cpu.c, 6);

        case 0x72: // BIT 6,D
            return bit(cpu.d, 6);

        case 0x73: // BIT 6,E
            return bit(cpu.e, 6);

        case 0x74: // BIT 6,H
            return bit(cpu.h, 6);

        case 0x75: // BIT 6,L
            return bit(cpu.l, 6);

        case 0x76: { // BIT 6,(HL)
            bit(memory_read8(cpu.hl), 6);
            return 12;
        }

        case 0x77: // BIT 6,A
            return bit(cpu.a, 6);

        case 0x78: // BIT 7,B
            return bit(cpu.b, 7);

        case 0x79: // BIT 7,C
            return bit(cpu.c, 7);

        case 0x7A: // BIT 7,D
            return bit(cpu.d, 7);

        case 0x7B: // BIT 7,E
            return bit(cpu.e, 7);

        case 0x7C: // BIT 7,H
            return bit(cpu.h, 7);

        case 0x7D: // BIT 7,L
            return bit(cpu.l, 7);

        case 0x7E: // BIT 7,(HL)
            bit(memory_read8(cpu.hl), 7);
            return 12;

        case 0x7F: // BIT 7,A
            return bit(cpu.a, 7);

        case 0x80: // RES 0,B
            return res(&cpu.b, 0);

        case 0x81: // RES 0,C
            return res(&cpu.c, 0);

        case 0x82: // RES 0,D
            return res(&cpu.d, 0);

        case 0x83: // RES 0,E
            return res(&cpu.e, 0);

        case 0x84: // RES 0,H
            return res(&cpu.h, 0);

        case 0x85: // RES 0,L
            return res(&cpu.l, 0);

        case 0x86: { // RES 0,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 0);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x87: // RES 0,A
            return res(&cpu.a, 0);

        case 0x88: // RES 1,B
            return res(&cpu.b, 1);

        case 0x89: // RES 1,C
            return res(&cpu.c, 1);

        case 0x8A: // RES 1,D
            return res(&cpu.d, 1);

        case 0x8B: // RES 1,E
            return res(&cpu.e, 1);

        case 0x8C: // RES 1,H
            return res(&cpu.h, 1);

        case 0x8D: // RES 1,L
            return res(&cpu.l, 1);

        case 0x8E: { // RES 1,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 1);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x8F: // RES 1,A
            return res(&cpu.a, 1);

        case 0x90: // RES 2,B
            return res(&cpu.b, 2);

        case 0x91: // RES 2,C
            return res(&cpu.c, 2);

        case 0x92: // RES 2,D
            return res(&cpu.d, 2);

        case 0x93: // RES 2,E
            return res(&cpu.e, 2);

        case 0x94: // RES 2,H
            return res(&cpu.h, 2);

        case 0x95: // RES 2,L
            return res(&cpu.l, 2);

        case 0x96: { // RES 2,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 2);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x97: // RES 2,A
            return res(&cpu.a, 2);

        case 0x98: // RES 3,B
            return res(&cpu.b, 3);

        case 0x99: // RES 3,C
            return res(&cpu.c, 3);

        case 0x9A: // RES 3,D
            return res(&cpu.d, 3);

        case 0x9B: // RES 3,E
            return res(&cpu.e, 3);

        case 0x9C: // RES 3,H
            return res(&cpu.h, 3);

        case 0x9D: // RES 3,L
            return res(&cpu.l, 3);

        case 0x9E: { // RES 3,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 3);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0x9F: // RES 3,A
            return res(&cpu.a, 3);

        case 0xA0: // RES 4,B
            return res(&cpu.b, 4);

        case 0xA1: // RES 4,C
            return res(&cpu.c, 4);

        case 0xA2: // RES 4,D
            return res(&cpu.d, 4);

        case 0xA3: // RES 4,E
            return res(&cpu.e, 4);

        case 0xA4: // RES 4,H
            return res(&cpu.h, 4);

        case 0xA5: // RES 4,L
            return res(&cpu.l, 4);

        case 0xA6: { // RES 4,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 4);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xA7: // RES 4,A
            return res(&cpu.a, 4);
            
        case 0xA8: // RES 5,B
            return res(&cpu.b, 5);

        case 0xA9: // RES 5,C
            return res(&cpu.c, 5);

        case 0xAA: // RES 5,D
            return res(&cpu.d, 5);

        case 0xAB: // RES 5,E
            return res(&cpu.e, 5);

        case 0xAC: // RES 5,H
            return res(&cpu.h, 5);

        case 0xAD: // RES 5,L
            return res(&cpu.l, 5);

        case 0xAE: { // RES 5,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 5);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xAF: // RES 5,A
            return res(&cpu.a, 5);

        case 0xB0: // RES 6,B
            return res(&cpu.b, 6);

        case 0xB1: // RES 6,C
            return res(&cpu.c, 6);

        case 0xB2: // RES 6,D
            return res(&cpu.d, 6);

        case 0xB3: // RES 6,E
            return res(&cpu.e, 6);

        case 0xB4: // RES 6,H
            return res(&cpu.h, 6);

        case 0xB5: // RES 6,L
            return res(&cpu.l, 6);

        case 0xB6: { // RES 6,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 6);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xB7: // RES 6,A
            return res(&cpu.a, 6);

        case 0xB8: // RES 7,B
            return res(&cpu.b, 7);

        case 0xB9: // RES 7,C
            return res(&cpu.c, 7);

        case 0xBA: // RES 7,D
            return res(&cpu.d, 7);

        case 0xBB: // RES 7,E
            return res(&cpu.e, 7);

        case 0xBC: // RES 7,H
            return res(&cpu.h, 7);

        case 0xBD: // RES 7,L
            return res(&cpu.l, 7);

        case 0xBE: { // RES 7,(HL)
            uint8_t value = memory_read8(cpu.hl);
            res(&value, 7);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xBF: // RES 7,A
            return res(&cpu.a, 7);

        case 0xC0: // SET 0,B
            return set(&cpu.b, 0);

        case 0xC1: // SET 0,C
            return set(&cpu.c, 0);

        case 0xC2: // SET 0,D
            return set(&cpu.d, 0);

        case 0xC3: // SET 0,E
            return set(&cpu.e, 0);

        case 0xC4: // SET 0,H
            return set(&cpu.h, 0);

        case 0xC5: // SET 0,L
            return set(&cpu.l, 0);

        case 0xC6: { // SET 0,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 0);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xC7: // SET 0,A
            return set(&cpu.a, 0);

        case 0xC8: // SET 1,B
            return set(&cpu.b, 1);

        case 0xC9: // SET 1,C
            return set(&cpu.c, 1);

        case 0xCA: // SET 1,D
            return set(&cpu.d, 1);

        case 0xCB: // SET 1,E
            return set(&cpu.e, 1);

        case 0xCC: // SET 1,H
            return set(&cpu.h, 1);

        case 0xCD: // SET 1,L
            return set(&cpu.l, 1);

        case 0xCE: { // SET 1,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 1);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xCF: // SET 1,A
            return set(&cpu.a, 1);

        case 0xD0: // SET 2,B
            return set(&cpu.b, 2);

        case 0xD1: // SET 2,C
            return set(&cpu.c, 2);

        case 0xD2: // SET 2,D
            return set(&cpu.d, 2);

        case 0xD3: // SET 2,E
            return set(&cpu.e, 2);

        case 0xD4: // SET 2,H
            return set(&cpu.h, 2);

        case 0xD5: // SET 2,L
            return set(&cpu.l, 2);

        case 0xD6: { // SET 2,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 2);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xD7: // SET 2,A
            return set(&cpu.a, 2);

        case 0xD8: // SET 3,B
            return set(&cpu.b, 3);

        case 0xD9: // SET 3,C
            return set(&cpu.c, 3);

        case 0xDA: // SET 3,D
            return set(&cpu.d, 3);

        case 0xDB: // SET 3,E
            return set(&cpu.e, 3);

        case 0xDC: // SET 3,H
            return set(&cpu.h, 3);

        case 0xDD: // SET 3,L
            return set(&cpu.l, 3);

        case 0xDE: { // SET 3,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 3);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xDF: // SET 3,A
            return set(&cpu.a, 3);

        case 0xE0: // SET 4,B
            return set(&cpu.b, 4);

        case 0xE1: // SET 4,C
            return set(&cpu.c, 4);

        case 0xE2: // SET 4,D
            return set(&cpu.d, 4);

        case 0xE3: // SET 4,E
            return set(&cpu.e, 4);

        case 0xE4: // SET 4,H
            return set(&cpu.h, 4);

        case 0xE5: // SET 4,L
            return set(&cpu.l, 4);

        case 0xE6: { // SET 4,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 4);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xE7: // SET 4,A
            return set(&cpu.a, 4);

        case 0xE8: // SET 5,B
            return set(&cpu.b, 5);

        case 0xE9: // SET 5,C
            return set(&cpu.c, 5);

        case 0xEA: // SET 5,D
            return set(&cpu.d, 5);

        case 0xEB: // SET 5,E
            return set(&cpu.e, 5);

        case 0xEC: // SET 5,H
            return set(&cpu.h, 5);

        case 0xED: // SET 5,L
            return set(&cpu.l, 5);

        case 0xEE: { // SET 5,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 5);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xEF: // SET 5,A
            return set(&cpu.a, 5);

        case 0xF0: // SET 6,B
            return set(&cpu.b, 6);

        case 0xF1: // SET 6,C
            return set(&cpu.c, 6);

        case 0xF2: // SET 6,D
            return set(&cpu.d, 6);

        case 0xF3: // SET 6,E
            return set(&cpu.e, 6);

        case 0xF4: // SET 6,H
            return set(&cpu.h, 6);

        case 0xF5: // SET 6,L
            return set(&cpu.l, 6);

        case 0xF6: { // SET 6,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 6);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xF7: // SET 6,A
            return set(&cpu.a, 6);

        case 0xF8: // SET 7,B
            return set(&cpu.b, 7);
        
        case 0xF9: // SET 7,C
            return set(&cpu.c, 7);

        case 0xFA: // SET 7,D
            return set(&cpu.d, 7);

        case 0xFB: // SET 7,E
            return set(&cpu.e, 7);

        case 0xFC: // SET 7,H
            return set(&cpu.h, 7);

        case 0xFD: // SET 7,L
            return set(&cpu.l, 7);

        case 0xFE: { // SET 7,(HL)
            uint8_t value = memory_read8(cpu.hl);
            set(&value, 7);
            memory_write8(cpu.hl, value);
            return 16;
        }

        case 0xFF: // SET 7,A
            return set(&cpu.a, 7);

        default:
            printf("Unimplemented opcode: 0xCB%02X at 0x%04X\n", opcode_cb, cpu.pc - 2);
            cpu.halted = 1;
            return 0;
    }
}
