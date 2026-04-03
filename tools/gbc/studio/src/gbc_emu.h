/*
 * gbc_emu.h — Game Boy Color emulator wrapper using igbc
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
void gbc_emu_read_mem(uint16_t addr, uint8_t *buf, int len);

/* Breakpoints */
void gbc_emu_add_bp(uint16_t addr);
void gbc_emu_remove_bp(uint16_t addr);
void gbc_emu_clear_bps(void);
int  gbc_emu_list_bps(uint16_t *out, int max);
void gbc_emu_set_bp_enabled(bool en);
bool gbc_emu_get_bp_enabled(void);
int  gbc_emu_get_last_bp(void); /* addr+1 or 0, clears after read */
bool gbc_emu_is_paused(void);
void gbc_emu_pause(void);
void gbc_emu_resume(void);
