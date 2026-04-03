/*
 * vectrex_emu.c — Vectrex emulator wrapper
 */

#include "vectrex_emu.h"
#include "vec2x/vec2x.h"
#include "vec2x/e8910.h"
#include "vec2x/e6809.h"
#include <stdio.h>
#include <string.h>

#define EMU_TIMER 20

/* Stub — vec2x.c calls this but we render ourselves */
void vec2x_platform_render(void) {}

static bool g_running = false;

/* CPU register access via e6809.h */

bool vemu_load(const char *rom_path, const char *cart_path) {
    FILE *f = fopen(rom_path, "rb");
    if (!f) { fprintf(stderr, "Can't open ROM: %s\n", rom_path); return false; }
    if (fread(rom, 1, sizeof(rom), f) != sizeof(rom)) {
        fprintf(stderr, "Invalid ROM size\n");
        fclose(f); return false;
    }
    fclose(f);

    memset(cart, 0, sizeof(cart));
    if (cart_path && cart_path[0]) {
        f = fopen(cart_path, "rb");
        if (!f) { fprintf(stderr, "Can't open cart: %s\n", cart_path); return false; }
        fread(cart, 1, sizeof(cart), f);
        fclose(f);
    }

    e8910_init_sound();
    vec2x_reset();
    g_running = true;
    return true;
}

void vemu_reset(void) {
    vec2x_reset();
}

void vemu_step(void) {
    if (!g_running) return;
    vec2x_emu((VECTREX_MHZ / 1000) * EMU_TIMER);
}

bool vemu_is_running(void) { return g_running; }

void vemu_stop(void) {
    if (g_running) {
        e8910_done_sound();
        g_running = false;
    }
}

void vemu_render(SDL_Renderer *renderer, int x, int y, int w, int h) {
    /* Scale vectors from ALG coords to screen rect */
    float scl_x = (float)w / ALG_MAX_X;
    float scl_y = (float)h / ALG_MAX_Y;
    float scl = scl_x < scl_y ? scl_x : scl_y;
    int off_x = x + (int)((w - ALG_MAX_X * scl) / 2);
    int off_y = y + (int)((h - ALG_MAX_Y * scl) / 2);

    for (long v = 0; v < vector_draw_cnt; v++) {
        unsigned char c = vectors_draw[v].color;
        if (c >= VECTREX_COLORS) continue;
        Uint8 intensity = (Uint8)(c * 255 / VECTREX_COLORS);
        SDL_SetRenderDrawColor(renderer, intensity, intensity, intensity, 255);

        int x0 = off_x + (int)(vectors_draw[v].x0 * scl);
        int y0 = off_y + (int)(vectors_draw[v].y0 * scl);
        int x1 = off_x + (int)(vectors_draw[v].x1 * scl);
        int y1 = off_y + (int)(vectors_draw[v].y1 * scl);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

void vemu_key_down(SDL_Keycode key) {
    switch (key) {
    case SDLK_a: snd_regs[14] &= ~0x01; break;
    case SDLK_s: snd_regs[14] &= ~0x02; break;
    case SDLK_d: snd_regs[14] &= ~0x04; break;
    case SDLK_f: snd_regs[14] &= ~0x08; break;
    case SDLK_LEFT:  alg_jch0 = 0x00; break;
    case SDLK_RIGHT: alg_jch0 = 0xff; break;
    case SDLK_UP:    alg_jch1 = 0xff; break;
    case SDLK_DOWN:  alg_jch1 = 0x00; break;
    default: break;
    }
}

void vemu_key_up(SDL_Keycode key) {
    switch (key) {
    case SDLK_a: snd_regs[14] |= 0x01; break;
    case SDLK_s: snd_regs[14] |= 0x02; break;
    case SDLK_d: snd_regs[14] |= 0x04; break;
    case SDLK_f: snd_regs[14] |= 0x08; break;
    case SDLK_LEFT:  alg_jch0 = 0x80; break;
    case SDLK_RIGHT: alg_jch0 = 0x80; break;
    case SDLK_UP:    alg_jch1 = 0x80; break;
    case SDLK_DOWN:  alg_jch1 = 0x80; break;
    default: break;
    }
}

VemuCpuState vemu_get_cpu(void) {
    VemuCpuState st;
    e6809_get_regs(&st.pc, &st.a, &st.b, &st.x, &st.y, &st.u, &st.s, &st.dp, &st.cc);
    return st;
}

long vemu_get_vector_count(void) { return vector_draw_cnt; }
