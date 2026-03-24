#include "memory.h"
#include "ppu.h"
#include "timer.h"
#include <string.h>
#include <stdio.h>
#include <raylib.h>

// === Fixed memory regions ===
static uint8_t vram[0x2000]; // 8KB
static uint8_t wram0[0x1000]; // 4KB
static uint8_t wram1[0x1000]; // 4KB
static uint8_t oam[0xA0]; // 160B
static uint8_t io_regs[0x80]; // 128B
static uint8_t hram[0x7F]; // 127B
static uint8_t interrupt_enable; // 1B
static uint8_t joypad_dir = 0x0F; // RIGHT, LEFT, UP, DOWN
static uint8_t joypad_action = 0x0F; // A, B, SELECT, START

// === MBC State & Memory ===
static uint8_t cartridge_ram[0x8000]; // 32KB of external RAM for saves
static uint8_t mbc_type = 0; // 0 = ROM ONLY, 1 = MBC1, 3 = MBC3
static uint32_t current_rom_bank = 1;
static uint8_t current_ram_bank = 0;
static uint8_t ram_enable = 0;
static uint8_t mbc1_bank1 = 1; 
static uint8_t mbc1_bank2 = 0;
static uint8_t mbc1_mode = 0; // 0 = ROM mode, 1 = RAM mode
static uint16_t mbc5_rom_bank = 1;

// === ROM data reference ===
static const uint8_t* full_rom_data = NULL;
static size_t full_rom_size = 0;

void update_mbc1_mapping() {
    current_rom_bank = (mbc1_bank2 << 5) | mbc1_bank1;
    if (mbc1_mode == 0) {
        current_ram_bank = 0;
    } else {
        current_ram_bank = mbc1_bank2;
    }
}

void memory_update_joypad(void) {
    uint8_t old_dir = joypad_dir;
    uint8_t old_act = joypad_action;

    // Reset all buttons to unpressed
    joypad_dir = 0x0F;
    joypad_action = 0x0F;

    // Direction keys
    if (IsKeyDown(KEY_RIGHT)) joypad_dir &= ~(1 << 0); // Clear Bit 0
    if (IsKeyDown(KEY_LEFT))  joypad_dir &= ~(1 << 1); // Clear Bit 1
    if (IsKeyDown(KEY_UP))    joypad_dir &= ~(1 << 2); // Clear Bit 2
    if (IsKeyDown(KEY_DOWN))  joypad_dir &= ~(1 << 3); // Clear Bit 3

    // Action keys (Z=A, X=B, Enter=Start, Backspace=Select)
    if (IsKeyDown(KEY_Z))         joypad_action &= ~(1 << 0); // Clear Bit 0
    if (IsKeyDown(KEY_X))         joypad_action &= ~(1 << 1); // Clear Bit 1
    if (IsKeyDown(KEY_BACKSPACE)) joypad_action &= ~(1 << 2); // Clear Bit 2
    if (IsKeyDown(KEY_ENTER))     joypad_action &= ~(1 << 3); // Clear Bit 3

    int req_int = 0;
    if (!(io_regs[0x00] & 0x20) && (joypad_action != old_act)) req_int = 1;
    if (!(io_regs[0x00] & 0x10) && (joypad_dir != old_dir)) req_int = 1;

    if (req_int) {
        uint8_t iflag = memory_read8(0xFF0F);
        memory_write8(0xFF0F, iflag | 0x10); 
    }
}

void memory_init() {
    memset(vram,       0x00, sizeof(vram));
    memset(wram0,      0x00, sizeof(wram0));
    memset(wram1,      0x00, sizeof(wram1));
    memset(oam,        0x00, sizeof(oam));
    memset(io_regs,    0x00, sizeof(io_regs));
    memset(hram,       0x00, sizeof(hram));
    memset(cartridge_ram, 0x00, sizeof(cartridge_ram));
    interrupt_enable = 0x00;
    io_regs[0x0F] = 0xE1;

    io_regs[0x00] = 0xF;  // Set the JOYP register (0xFF00) to 0xF (no buttons pressed)
}

void memory_load_rom(const uint8_t* rom_data, size_t rom_size) {
    full_rom_data = rom_data;
    full_rom_size = rom_size;

    // Detect MBC Type from Header
    uint8_t cart_type = rom_data[0x0147];
    if (cart_type == 0x00) mbc_type = 0; // ROM ONLY
    else if (cart_type >= 0x01 && cart_type <= 0x03) mbc_type = 1; // MBC1
    else if (cart_type == 0x05 || cart_type == 0x06) mbc_type = 2; // MBC2
    else if (cart_type >= 0x0F && cart_type <= 0x13) mbc_type = 3; // MBC3
    else if (cart_type >= 0x19 && cart_type <= 0x1E) mbc_type = 5; // MBC5

    // Reset Banks
    current_rom_bank = 1;
    mbc5_rom_bank = 1;
    current_ram_bank = 0;
    ram_enable = 0;
    mbc1_mode = 0;
}

uint8_t memory_read8(uint16_t addr) {
    if (addr <= 0x3FFF) {
        // Bank 0
        if (mbc_type == 1 && mbc1_mode == 1) {
            uint32_t zero_bank = mbc1_bank2 << 5;
            uint32_t mapped_addr = (zero_bank * 0x4000) + addr;
            if (mapped_addr < full_rom_size) return full_rom_data[mapped_addr];
            return 0xFF;
        }
        return full_rom_data[addr];
    } else if (addr <= 0x7FFF) {
        // Switchable ROM Bank
        uint32_t mapped_addr = (current_rom_bank * 0x4000) + (addr - 0x4000);
        if (mapped_addr < full_rom_size) return full_rom_data[mapped_addr];
        return 0xFF;
    } else if (addr <= 0x9FFF) {
        return vram[addr - 0x8000];
    } else if (addr <= 0xBFFF) {
        // Switchable External RAM
        if (!ram_enable) return 0xFF; 
        
        if (mbc_type == 2) {
            // MBC2 built-in RAM
            return cartridge_ram[addr & 0x01FF] | 0xF0;
        } else {
            // MBC1 / MBC3 standard external RAM
            uint32_t mapped_addr = (current_ram_bank * 0x2000) + (addr - 0xA000);
            return cartridge_ram[mapped_addr];
        }
    } else if (addr <= 0xCFFF) {
        return wram0[addr - 0xC000];
    } else if (addr <= 0xDFFF) {
        return wram1[addr - 0xD000];
    } else if (addr <= 0xFDFF) {
        return memory_read8(addr - 0x2000); // Echo RAM
    } else if (addr <= 0xFE9F) {
        return oam[addr - 0xFE00];
    } else if (addr <= 0xFEFF) {
        return 0xFF; // Not usable
    } else if (addr <= 0xFF7F) {

        if (addr == 0xFF00) {
            uint8_t req = io_regs[0x00];
            uint8_t out = 0xC0 | (req & 0x30) | 0x0F;

            // if bit 5 is 0, Action buttons are selected
            if (!(req & 0x20)) {
                out &= joypad_action;
            }
            // if bit 4 is 0, Direction buttons are selected
            if (!(req & 0x10)) {
                out &= joypad_dir;
            }
            return out;
        }

        if (addr == 0XFF0F) {
            return io_regs[0x0F];
        }

        if (addr >= 0xFF40 && addr <= 0xFF4B){
            return ppu_read(addr);
        } else {
            return io_regs[addr - 0xFF00];
        }

    } else if (addr <= 0xFFFE) {
        return hram[addr - 0xFF80];
    } else if (addr == 0xFFFF) {
        return interrupt_enable;
    }

    return 0xFF;
}

void memory_write8(uint16_t addr, uint8_t value) {
    if (addr <= 0x1FFF) {
        // Enable RAM
        if (mbc_type == 1 || mbc_type == 3 || mbc_type == 5) {
            ram_enable = ((value & 0x0F) == 0x0A);
        } else if (mbc_type == 2) {
            // MBC2 RAM Enable (iff bit 8 of addr is 0)
            if ((addr & 0x0100) == 0) {
                ram_enable = ((value & 0x0F) == 0x0A);
            }
        }
    } else if (addr <= 0x3FFF) {
        // MBC CONTROL
        // ROM Bank Number (Lower 5 bits)
        if (mbc_type == 1) {
            mbc1_bank1 = value & 0x1F;
            if (mbc1_bank1 == 0) mbc1_bank1 = 1;
            update_mbc1_mapping();
        } else if (mbc_type == 3) {
            value &= 0x7F;
            if (value == 0) value = 1;
            current_rom_bank = value;
        } else if (mbc_type == 2) {
            // MBC2 ROM bank iff bit 8 of addr is 1
            if ((addr & 0x0100) != 0) {
                value &= 0x0F; // MBC2 only uses 4 bits for ROM bank
                if (value == 0) value = 1;
                current_rom_bank = value;
            }
        } else if (mbc_type == 5) {
            // MBC5 splits the ROM bank register into two pieces!
            if (addr <= 0x2FFF) {
                // Lower 8 bits
                mbc5_rom_bank = (mbc5_rom_bank & 0x0100) | value;
            } else {
                // Upper 9th bit (Bit 0 of the value)
                mbc5_rom_bank = (mbc5_rom_bank & 0x00FF) | ((value & 0x01) << 8);
            }
            // MBC5 allows bank 0 to be mapped to 0x4000! No zero-hack needed.
            current_rom_bank = mbc5_rom_bank;
        }
    } else if (addr <= 0x5FFF) {
        // RAM Bank Number or Upper ROM Bank Bits
        if (mbc_type == 1) {
            mbc1_bank2 = value & 0x03;
            update_mbc1_mapping();
        } else if (mbc_type == 3) {
            if (value <= 0x03) current_ram_bank = value;
        } else if (mbc_type == 5) {
            // MBC5 supports up to 16 RAM banks (Full 4 bits)
            current_ram_bank = value & 0x0F;
        }
    } else if (addr <= 0x7FFF) {
        // MBC ROM bank switching
        // // Banking Mode Select (MBC1)
        if (mbc_type == 1) {
            mbc1_mode = value & 0x01;
            update_mbc1_mapping();
        }
    } else if (addr <= 0x9FFF) {
        vram[addr - 0x8000] = value;
    } else if (addr <= 0xBFFF) {
        // Write to External RAM
        if (ram_enable) {
            if (mbc_type == 2) {
                // MBC2 built-in RAM: Only store the lower 4 bits
                cartridge_ram[addr & 0x01FF] = value & 0x0F;
            } else {
                // MBC1 / MBC3 / MBC5 standard external RAM
                uint32_t mapped_addr = (current_ram_bank * 0x2000) + (addr - 0xA000);
                cartridge_ram[mapped_addr] = value;
            }
        }
    } else if (addr <= 0xCFFF) {
        wram0[addr - 0xC000] = value;
    } else if (addr <= 0xDFFF) {
        wram1[addr - 0xD000] = value;
    } else if (addr <= 0xFDFF) {
        memory_write8(addr - 0x2000, value); // Echo RAM
    } else if (addr <= 0xFE9F) {
        oam[addr - 0xFE00] = value;
    } else if (addr <= 0xFEFF) {
        // Not usable
    } else if (addr <= 0xFF7F) {    
        if (addr == 0xFF00) {
            io_regs[0x00] = (io_regs[0x00] & 0xCF) | (value & 0x30);
            return;
        }

        // --- TIMER SPECIAL CASE ---
        if (addr == 0xFF04) { // DIV register
            io_regs[addr - 0xFF00] = 0; // writing resets DIV (see pandocs)
            timer_reset_internal_div(); // Reset internal clock tick
            return;
        }

        if (addr == 0xFF0F) {
            io_regs[addr - 0xFF00] = value;
        } else {
            io_regs[addr - 0xFF00] = value;
            ppu_write(addr, value); // Handle LCDC side effects
        }
    } else if (addr <= 0xFFFE) {
        hram[addr - 0xFF80] = value;
    } else if (addr == 0xFFFF) {
        interrupt_enable = value;
    }
}

uint8_t* memory_get_oam(void) {
    return oam;  // Returns a pointer to the static OAM array
}

void memory_set_div_direct(uint8_t value) {
    io_regs[0x04] = value; // Update the register directly without resetting it, only to be used in the timer as I dont want any side effects of the cpu;
}
