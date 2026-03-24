#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

// Memory API
void memory_init();
void memory_load_rom(const uint8_t* rom_data, size_t rom_size);
uint8_t memory_read8(uint16_t address);
void memory_write8(uint16_t address, uint8_t value);
uint8_t* memory_get_oam(void);
void joyp_set_buttons(uint8_t new_state);
void memory_update_joypad(void);
void memory_set_div_direct(uint8_t value);
void update_mbc1_mapping(void);

#endif // MEMORY_H
