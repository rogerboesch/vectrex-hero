/*
 * vectrex_emu.h — Vectrex emulator wrapper for embedded use
 *
 * Wraps vec2x: load ROM/cart, run cycles, render vectors to SDL renderer.
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>

bool vemu_load(const char *rom_path, const char *cart_path);
void vemu_reset(void);
void vemu_step(void);  /* run one frame (~20ms worth of cycles) */
bool vemu_is_running(void);
void vemu_stop(void);

/* Render vector lines into a rect on the given renderer */
void vemu_render(SDL_Renderer *renderer, int x, int y, int w, int h);

/* Input: buttons (bitmask) and joystick */
void vemu_key_down(SDL_Keycode key);
void vemu_key_up(SDL_Keycode key);

/* Memory access */
unsigned char vemu_read8(unsigned addr);
void vemu_read_mem(unsigned addr, unsigned char *buf, int len);

/* CPU state for debug */
typedef struct {
    unsigned pc, a, b, x, y, u, s, dp, cc;
} VemuCpuState;
VemuCpuState vemu_get_cpu(void);
long vemu_get_vector_count(void);
