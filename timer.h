#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init();
void timer_step(int cycles);
void timer_reset_internal_div();

#endif