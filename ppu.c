#include "ppu.h"
#include "memory.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <raylib.h>

// LCD Control Register structure (0xFF40)
typedef struct {
    uint8_t lcd_enabled; // Bit 7
    uint8_t window_tile_map_select; // Bit 6
    uint8_t window_enabled; // Bit 5
    uint8_t bg_tile_data_select; // Bit 4
    uint8_t bg_tile_map_select; // Bit 3
    uint8_t obj_size; // Bit 2
    uint8_t obj_enabled; // Bit 1
    uint8_t bg_enabled; // Bit 0
} LCDControl;

static LCDControl lcdc;
static uint8_t current_line = 0;
static uint8_t scy = 0x00;
static uint8_t scx = 0x00;
static uint8_t stat_register = 0x00; // bits 7-4 interrupt enables, bits 3-0 partially readonly/flags
static uint8_t lyc_register = 0x00; // LY Compare (0xFF45)
static uint8_t current_mode = 0; // PPU mode (0-3)

static uint32_t dma_cycles_left = 0; // Tracks remaining DMA cycles
static uint8_t dma_in_progress = 0; // Flag to indicate if DMA is currently in progress

uint8_t bgp_register = 0xFC;

// The object palette (for palette 0)
static uint8_t obj_palette_0[4]; // Array of 4 color values

// The object palette (for palette 1)
static uint8_t obj_palette_1[4]; // Array of 4 color values for Object Palette 1


static const uint32_t color_palette[4] = {
    0xFFFFFF, // White (0x00)
    0xBDBDBD, // Light Gray (0x01)
    0x6E6E6E, // Dark Gray (0x02)
    0x000000 // Black (0x03)
};

// Framebuffer for 160x144 pixels
static Color framebuffer[160 * 144];

// Game Boy palette to Raylib colors
static const Color gb_colors[4] = {
    { 0xEF, 0xFF, 0xD7, 0xFF }, // White (00)
    { 0xAD, 0xD7, 0x94, 0xFF }, // Light Gray (01)
    { 0x52, 0x84, 0x31, 0xFF }, // Dark Gray (10)
    { 0x08, 0x4A, 0x10, 0xFF }  // Black (11)
};

static uint8_t wx = 0;
static uint8_t wy = 0;

void ppu_init(void) {
    memset(&lcdc, 0, sizeof(lcdc));
    memset(framebuffer, 0, sizeof(framebuffer));
    for (int i = 0; i < 160 * 144; i++) {
        framebuffer[i] = gb_colors[0]; // Initialize to white
    }
    memset(obj_palette_0, 0, sizeof(obj_palette_0));
    memset(obj_palette_1, 0, sizeof(obj_palette_1));
    current_line = 0;
    scy = 0x00;
    scx = 0x00;
    stat_register = 0x00;
    lyc_register = 0x00;
    current_mode = 0;
    dma_cycles_left = 0;
    dma_in_progress = 0;
    bgp_register = 0xFC;

    memory_write8(0xFF40, 0x91); // LCDC: LCD is ON, BG is ON
    memory_write8(0xFF47, 0xFC); // BGP: Standard Palette
    memory_write8(0xFF41, 0x85); // STAT
}

uint8_t* ppu_get_framebuffer(void) {
    return (uint8_t*)framebuffer;
}

int ppu_is_vblank(void) {
    return current_line == 144;
}

void ppu_update_lcdc(uint8_t value) {
    lcdc.lcd_enabled             = (value >> 7) & 1;
    lcdc.window_tile_map_select = (value >> 6) & 1;
    lcdc.window_enabled          = (value >> 5) & 1;
    lcdc.bg_tile_data_select     = (value >> 4) & 1;
    lcdc.bg_tile_map_select      = (value >> 3) & 1;
    lcdc.obj_size                = (value >> 2) & 1;
    lcdc.obj_enabled             = (value >> 1) & 1;
    lcdc.bg_enabled              =  value       & 1;
}

void ppu_write(uint16_t addr, uint8_t value) {
    if (addr == 0xFF40) {
        ppu_update_lcdc(value);
    } else if (addr == 0xFF41) {
        // Only bits 7-4 writable interrupt enables, lower bits read-only
        stat_register = (value & 0xF8) | (stat_register & 0x07);
    } else if (addr == 0xFF42) {
        scy = value;
        ///printf("SCY set to 0x%02X\n", value);
    } else if (addr == 0xFF43) {
        scx = value;
        ///printf("SCX set to 0x%02X\n", value);
    } else if (addr == 0xFF44) {
        current_line = 0; // Writing to LY resets it
    } else if (addr == 0xFF45) {
        lyc_register = value;
    } else if (addr == 0xFF46) {
        uint16_t source_addr = value << 8; // Extract high byte for the source address (value * 0x100)

        uint8_t* oam_ptr = memory_get_oam();
        
        // DMA: Copy 160 bytes from memory to OAM
        for (int i = 0; i < 160; i++) {
            oam_ptr[i] = memory_read8(source_addr + i);  // Copying byte by byte
        }

        // Set the DMA cycle counter
        dma_cycles_left = 160;
        dma_in_progress = 1;
    } else if (addr == 0xFF47) {
        bgp_register = value; // Write to background palette register
    } else if (addr == 0xFF48) {
        // Update Object Palette 0
        obj_palette_0[0] = value & 0x03; // Color 0 (Bits 0-1)
        obj_palette_0[1] = (value >> 2) & 0x03; // Color 1 (Bits 2-3)
        obj_palette_0[2] = (value >> 4) & 0x03; // Color 2 (Bits 4-5)
        obj_palette_0[3] = (value >> 6) & 0x03; // Color 3 (Bits 6-7)
    } else if (addr == 0xFF49) {
        // Update Object Palette 1
        obj_palette_1[0] = value & 0x03; // Color 0 (Bits 0-1)
        obj_palette_1[1] = (value >> 2) & 0x03; // Color 1 (Bits 2-3)
        obj_palette_1[2] = (value >> 4) & 0x03; // Color 2 (Bits 4-5)
        obj_palette_1[3] = (value >> 6) & 0x03; // Color 3 (Bits 6-7)
    } else if (addr == 0xFF4A) {
        wy = value;
    } else if (addr == 0xFF4B) {
        wx = value;
    }
}

uint8_t ppu_read(uint16_t addr) {
    switch (addr) {
        case 0xFF40: {
            uint8_t value = 0;
            value |= lcdc.lcd_enabled             << 7;
            value |= lcdc.window_tile_map_select << 6;
            value |= lcdc.window_enabled          << 5;
            value |= lcdc.bg_tile_data_select     << 4;
            value |= lcdc.bg_tile_map_select      << 3;
            value |= lcdc.obj_size                << 2;
            value |= lcdc.obj_enabled             << 1;
            value |= lcdc.bg_enabled;
            return value;
        }

        case 0xFF41: {
            uint8_t stat = stat_register & 0xFC; // Keep writable bits and coincidence flag (bits 7-2)
            stat |= (current_mode & 0x03); // Bits 1-0 = current PPU mode
            return stat | 0x80;
        }

        case 0xFF42: // SCY
            return scy;

        case 0xFF43: // SCX
            return scx;

        case 0xFF44: // LY (Current Scanline)
            return current_line; //0x90; // for gameboy doctor //current_line;

        case 0xFF45: // LYC
            return lyc_register;
        
        case 0xFF47:
            return bgp_register;

        case 0xFF48:
            // Return Object Palette 0 as a single byte
            return (obj_palette_0[0] << 6) | (obj_palette_0[1] << 4) | (obj_palette_0[2] << 2) | obj_palette_0[3];

        case 0xFF49:
        // Return Object Palette 1 as a single byte
        return (obj_palette_1[0] << 6) | (obj_palette_1[1] << 4) | (obj_palette_1[2] << 2) | obj_palette_1[3];

        case 0xFF4A: return wy;

        case 0xFF4B: return wx;

        default:
            return 0xFF;
    }
}

void ppu_step(uint32_t cycles) {
    static uint32_t scanline_cycles = 0;
    static uint8_t previous_mode = 0;

    scanline_cycles += cycles;

    if (!lcdc.lcd_enabled) {
        current_line = 0;
        current_mode = 0;
        scanline_cycles = 0;
        return;
    }

    // If DMA is in progress, decrement the remaining cycles
    if (dma_in_progress) {
        dma_cycles_left -= cycles;
        if (dma_cycles_left <= 0) {
            dma_in_progress = 0;  // DMA completed
        }
    }

    // Determine the new PPU mode based on current_line and scanline_cycles
    uint8_t new_mode = current_mode;
    if (current_line >= 144) {
        new_mode = 1; // VBlank
    } else {
        if (scanline_cycles <= 80) {
            new_mode = 2; // OAM
        } else if (scanline_cycles <= 252) {
            new_mode = 3; // Drawing
        } else {
            new_mode = 0; // HBlank
        }
    }

    // Handle mode transitions
    if (new_mode != previous_mode) {
        current_mode = new_mode;

        // Render the scanline exactly ONCE when entering HBlank (Mode 0)
        if (current_mode == 0) {
            render_background_line(current_line);
        }

        // Fire LCD STAT interrupts for mode changes
        int req_int = 0;
        if (current_mode == 0 && (stat_register & (1 << 3))) req_int = 1; // HBlank int enable
        if (current_mode == 1 && (stat_register & (1 << 4))) req_int = 1; // VBlank int enable
        if (current_mode == 2 && (stat_register & (1 << 5))) req_int = 1; // OAM int enable

        if (req_int) {
            uint8_t iflag = memory_read8(0xFF0F);
            memory_write8(0xFF0F, iflag | 0x02); // Request STAT interrupt (Bit 1)
        }
        
        previous_mode = current_mode;
    }

    // if a full scanline has passed (456 cycles)
    if (scanline_cycles >= 456) {  
        scanline_cycles -= 456;
        current_line++;

        if (current_line > 153) {
            current_line = 0;
        }

        // LY == LYC Coincidence Check
        if (current_line == lyc_register) {
            stat_register |= (1 << 2); // Set Coincidence Flag (Bit 2)
            if (stat_register & (1 << 6)) { // If LY=LYC Interrupt is enabled (Bit 6)
                uint8_t iflag = memory_read8(0xFF0F);
                memory_write8(0xFF0F, iflag | 0x02); // Fire STAT interrupt
            }
        } else {
            stat_register &= ~(1 << 2); // Clear Coincidence Flag
        }

        // this will trigger VBlank interrupt at line 144
        if (current_line == 144) {
            uint8_t iflag = memory_read8(0xFF0F);
            memory_write8(0xFF0F, iflag | 0x01); // VBlank interrupt request
        }
    }
}

void render_background_line(uint8_t line) {
    if (!lcdc.lcd_enabled) {
        for (int x = 0; x < 160; x++) {
            framebuffer[line * 160 + x] = gb_colors[0]; // White
        }
        ///printf("Line %d: LCD disabled (LCDC=0x%02X)\n", line, ppu_read(0xFF40));
        return;
    }

    // will clear line to white if background and sprites are disabled
    if (!lcdc.bg_enabled && !lcdc.obj_enabled) {
        for (int x = 0; x < 160; x++) {
            framebuffer[line * 160 + x] = gb_colors[0]; // White
        }
        ///printf("Line %d: BG and OBJ disabled (LCDC=0x%02X)\n", line, ppu_read(0xFF40));
        return;
    }

    // Initialize line to transparent, sprites
    if (lcdc.bg_enabled) {
        for (int x = 0; x < 160; x++) {
            framebuffer[line * 160 + x] = gb_colors[0]; // White
        }
    }

    // Draw Background and Window
    if (lcdc.bg_enabled) {
        int window_visible = 0;
        if (lcdc.window_enabled && line >= wy) {
            window_visible = 1;
        }

        uint16_t tile_data_base = lcdc.bg_tile_data_select ? 0x8000 : 0x9000;

        for (int x = 0; x < 160; x++) {
            uint16_t tile_map_base;
            uint8_t y_pos;
            uint8_t x_pos;

            // WX is offset by 7, then if WX is 7 window starts at x=0.
            int window_x = x + 7 - wx; 

            if (window_visible && window_x >= 0) {
                tile_map_base = lcdc.window_tile_map_select ? 0x9C00 : 0x9800;
                y_pos = line - wy; // Window has its own internal Y
                x_pos = (uint8_t)window_x; // Window has its own internal X
            } else {
                tile_map_base = lcdc.bg_tile_map_select ? 0x9C00 : 0x9800;
                y_pos = (line + scy) & 0xFF; // Background scrolls
                x_pos = (x + scx) & 0xFF;
            }

            // to find which tile we are on
            uint8_t tile_y = y_pos >> 3;
            uint8_t tile_x = x_pos >> 3;
            uint16_t tile_map_addr = tile_map_base + (tile_y * 32) + tile_x;
            uint8_t tile_index = memory_read8(tile_map_addr);

            // Get the pixel data from the tile
            uint8_t pixel_y = y_pos & 7;
            uint16_t tile_addr = lcdc.bg_tile_data_select ?
                                 tile_data_base + (tile_index * 16) :
                                 tile_data_base + ((int8_t)tile_index * 16);
            
            uint16_t tile_row_addr = tile_addr + (pixel_y * 2);
            uint8_t low_byte = memory_read8(tile_row_addr);
            uint8_t high_byte = memory_read8(tile_row_addr + 1);

            uint8_t bit = 7 - (x_pos & 7);
            uint8_t low_bit = (low_byte >> bit) & 1;
            uint8_t high_bit = (high_byte >> bit) & 1;
            uint8_t color_id = (high_bit << 1) | low_bit;

            uint8_t palette_color = (bgp_register >> (color_id * 2)) & 0x03;
            framebuffer[line * 160 + x] = gb_colors[palette_color];
        }
    }

    // Sprite rendering
    if (lcdc.obj_enabled) {
        uint8_t* oam = memory_get_oam();
        uint8_t sprite_height = lcdc.obj_size ? 16 : 8;
        for (int i = 0; i < 40; i++) {
            uint8_t y_pos = oam[i * 4] - 16;
            uint8_t x_pos = oam[i * 4 + 1] - 8;
            uint8_t tile_index = oam[i * 4 + 2];
            uint8_t attributes = oam[i * 4 + 3];

            if (line >= y_pos && line < y_pos + sprite_height) {
                uint8_t pixel_y = line - y_pos;
                if (attributes & (1 << 6)) {
                    pixel_y = sprite_height - 1 - pixel_y;
                }
                if (sprite_height == 16 && pixel_y >= 8) {
                    tile_index |= 1;
                    pixel_y -= 8;
                }
                uint16_t tile_addr = 0x8000 + (tile_index * 16) + (pixel_y * 2);
                uint8_t low_byte = memory_read8(tile_addr);
                uint8_t high_byte = memory_read8(tile_addr + 1);

                uint8_t* palette = (attributes & (1 << 4)) ? obj_palette_1 : obj_palette_0;
                for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
                    int screen_x = x_pos + pixel_x;
                    if (screen_x < 0 || screen_x >= 160) continue;
                    uint8_t bit = 7 - pixel_x;
                    if (attributes & (1 << 5)) {
                        bit = pixel_x;
                    }
                    uint8_t color_id = ((high_byte >> bit) & 1) << 1 | ((low_byte >> bit) & 1);
                    if (color_id == 0) continue; // transparent
                    if (!(attributes & (1 << 7)) || framebuffer[line * 160 + screen_x].r == gb_colors[0].r) {
                        uint8_t palette_color = palette[color_id];
                        framebuffer[line * 160 + screen_x] = gb_colors[palette_color];
                    }
                }
            }
        }
    }
}


void print_palette(uint8_t palette_register) {
    for (int i = 0; i < 4; i++) {
        uint8_t color_index = (palette_register >> (2 * i)) & 0x03;
    }
}
