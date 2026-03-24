#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

uint8_t* rom_data = NULL;
size_t rom_size = 0;
RomInfo rom_info;

int load_rom(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open ROM file");
        return 0;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (rom_size > MAX_ROM_SIZE) {
        printf("ROM too large: %zu bytes\n", rom_size);
        fclose(f);
        return 0;
    }

    rom_data = (uint8_t*)malloc(rom_size);
    if (!rom_data) {
        perror("Failed to allocate ROM buffer");
        fclose(f);
        return 0;
    }

    fread(rom_data, 1, rom_size, f);
    fclose(f);

    rom_info.rom_size_bytes = rom_size;
    return 1;
}

void free_rom() {
    if (rom_data) {
        free(rom_data);
        rom_data = NULL;
        rom_size = 0;
    }
}

void parse_rom_header() {
    // Game title (0x0134-0x0143)
    memset(rom_info.title, 0, sizeof(rom_info.title));
    memcpy(rom_info.title, &rom_data[0x0134], 16);
    rom_info.title[16] = '\0';

    // Cartridge type (0x0147)
    rom_info.type = rom_data[0x0147];

    // ROM size code (0x0148)
    rom_info.rom_size_code = rom_data[0x0148];

    // RAM size code (0x0149)
    rom_info.ram_size_code = rom_data[0x0149];

    printf("ROM Title: %s\n", rom_info.title);
    printf("Cartridge Type: 0x%02X\n", rom_info.type);
    printf("ROM Size Code: 0x%02X\n", rom_info.rom_size_code);
    printf("RAM Size Code: 0x%02X\n", rom_info.ram_size_code);
}

void print_rom_contents() {
    if (!rom_data || rom_size == 0) {
        printf("ROM not loaded.\n");
        return;
    }

    printf("=== ROM Dump (%zu bytes) ===\n", rom_size);

    for (size_t i = 0; i < rom_size; i += 16) {
        printf("%06zx: ", i);

        // Print hex bytes
        for (int j = 0; j < 16; ++j) {
            if (i + j < rom_size)
                printf("%02X ", rom_data[i + j]);
            else
                printf("   ");
        }

        printf(" | ");

        // Print ASCII characters
        for (int j = 0; j < 16; ++j) {
            if (i + j < rom_size) {
                char c = rom_data[i + j];
                printf("%c", isprint(c) ? c : '.');
            }
        }

        printf("\n");
    }
}
