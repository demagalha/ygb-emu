#include <stdio.h>
#include <raylib.h>
#include "rom.h"
#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144
#define MAX_CYCLES_PER_FRAME 70244 // 154 (scan lines) * 456 (cpu clock cycles)
#define SCALE_FACTOR 4
#define WINDOW_WIDTH (GB_SCREEN_WIDTH * SCALE_FACTOR)
#define WINDOW_HEIGHT (GB_SCREEN_HEIGHT * SCALE_FACTOR)

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <rom>\n", argv[0]);
        return 1;
    }

    if (!load_rom(argv[1])) return 1;

    memory_init();
    memory_load_rom(rom_data, rom_size);
    ppu_init();
    timer_init();
    cpu_reset();
    
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Game Boy Emulator");
    SetTargetFPS(60); // ~59.7 Hz for Game Boy

    // Texture for the Game Boy screen
    Image screen_image = GenImageColor(GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, BLACK);
    Texture2D screen_texture = LoadTextureFromImage(screen_image);
    UnloadImage(screen_image);

    while (!WindowShouldClose()) {
        memory_update_joypad();
        int cycles_this_frame = 0;

        while (cycles_this_frame < MAX_CYCLES_PER_FRAME) {
            int cycles = cpu_step();
            timer_step(cycles);
            ppu_step(cycles);
            cycles_this_frame += cycles;
        }
        // Now that the frame is fully generated in memory, draw it ONCE
        BeginDrawing();
        ClearBackground(BLACK);
        UpdateTexture(screen_texture, ppu_get_framebuffer());
        DrawTextureEx(screen_texture, (Vector2){0, 0}, 0.0f, SCALE_FACTOR, WHITE);
        EndDrawing();
    }

    UnloadTexture(screen_texture);
    CloseWindow();
    free_rom();
    return 0;
}
