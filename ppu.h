#ifndef PPU_H
#define PPU_H

#include <stdint.h>

void ppu_init(void);
void ppu_write(uint16_t addr, uint8_t value);
uint8_t ppu_read(uint16_t addr);
void ppu_step(uint32_t cycles);
extern uint8_t bgp_register;
void print_palette(uint8_t palette_register);
uint8_t* ppu_get_framebuffer(void);
int ppu_is_vblank(void);
void render_background_line(uint8_t line);
void ppu_update_lcdc(uint8_t value);

#endif
