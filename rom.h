#ifndef ROM_H
#define ROM_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ROM_SIZE 0x800000  // 8MB max
#define ROM_HEADER_START 0x0100
#define ROM_HEADER_END   0x014F

// ROM metadata
typedef struct {
    char title[17];    // 16 chars + null terminator
    uint8_t type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;
    size_t rom_size_bytes;
} RomInfo;

// Global ROM data
extern uint8_t* rom_data;
extern size_t rom_size;
extern RomInfo rom_info;

// Load ROM file into memory
int load_rom(const char* filename);
void free_rom();

// Parse header data
void parse_rom_header();

void print_rom_contents();

#endif // ROM_H
