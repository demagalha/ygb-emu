#ifndef CPU_H
#define CPU_H

#include <stdint.h>

// Flag bit masks
#define FLAG_Z 0x80  // Zero
#define FLAG_N 0x40  // Subtract
#define FLAG_H 0x20  // Half Carry
#define FLAG_C 0x10  // Carry

// Set a flag
#define SET_FLAG(flag)   (cpu.f |= (flag))
// Clear a flag
#define CLEAR_FLAG(flag) (cpu.f &= ~(flag))
// Check if flag is set (returns 0 or non-zero)
#define CHECK_FLAG(flag) (cpu.f & (flag))
// Write flag based on condition (if cond true set, else clear)
#define WRITE_FLAG(flag, cond) \
    do { \
        if (cond) SET_FLAG(flag); else CLEAR_FLAG(flag); \
    } while(0)


typedef struct {
    union {
        struct {
            uint8_t f;
            uint8_t a;
        };
        uint16_t af;
    };
    union {
        struct {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };
    union {
        struct {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };
    union {
        struct {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };

    uint16_t sp;  // Stack Pointer
    uint16_t pc;  // Program Counter

    int halted;
    int ime; // Interrupt Master Enable
    int ime_pending;
    int just_interrupted;
} CPU;

extern CPU cpu;

void cpu_reset();
int cpu_step();
void print_flags();
int execute_cb_instruction(uint8_t opcode_cb);
int handle_interrupts();

#endif
