#include "timer.h"
#include "memory.h"

static int div_counter = 0;
static int tima_counter = 0;

static int timer_period(uint8_t tac) {
    switch (tac & 0x3) {
        case 0: return 1024;
        case 1: return 16;
        case 2: return 64;
        case 3: return 256;
    }
    return 1024;
}

void timer_init() {
    div_counter = 0;
    tima_counter = 0;
}

void timer_reset_internal_div() {
    div_counter = 0;
}

void timer_step(int cycles) {

    // ----- DIV -----
    div_counter += cycles;

    while (div_counter >= 256) {
        div_counter -= 256;

        uint8_t div = memory_read8(0xFF04);
        memory_set_div_direct(div + 1); // memory_write8(0xFF04, div + 1); if we use this this will reset div...
    }

    // ----- TIMA -----
    uint8_t tac = memory_read8(0xFF07);

    if (!(tac & 0x04)) return; // timer disabled

    int period = timer_period(tac);

    tima_counter += cycles;

    while (tima_counter >= period) {
        tima_counter -= period;

        uint8_t tima = memory_read8(0xFF05);

        if (tima == 0xFF) {
            uint8_t tma = memory_read8(0xFF06);
            memory_write8(0xFF05, tma);

            uint8_t iflag = memory_read8(0xFF0F);
            memory_write8(0xFF0F, iflag | 0x04); // request timer interrupt
        } else {
            memory_write8(0xFF05, tima + 1);
        }
    }
}