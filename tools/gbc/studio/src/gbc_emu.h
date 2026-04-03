/*
 * gbc_emu.h — Game Boy Color emulator wrapper using gaembuoy
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

bool gbc_emu_load(SDL_Renderer *renderer, const char *rom_path);
void gbc_emu_stop(void);
bool gbc_emu_is_running(void);
void gbc_emu_step(void);  /* run ~1/120s of cycles */
void gbc_emu_render(SDL_Renderer *renderer, int x, int y, int w, int h);

/* Input */
void gbc_emu_key_down(SDL_Keycode key);
void gbc_emu_key_up(SDL_Keycode key);

/* CPU state for debug */
typedef struct {
    uint16_t pc, sp, af, bc, de, hl;
} GbcCpuState;
GbcCpuState gbc_emu_get_cpu(void);

/* Memory */
uint8_t gbc_emu_read8(uint16_t addr);
