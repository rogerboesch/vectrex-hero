/*
 * vectrex_emu.c — Vectrex emulator wrapper
 */

#include "vectrex_emu.h"
#include "../ivec/vec2x.h"
#include "../ivec/e8910.h"
#include "../ivec/e6809.h"
#include <stdio.h>
#include <string.h>

/* Vectrex runs at 1.5 MHz. At 60fps display, that's 25000 cycles per frame.
 * The vec2x internal frame (phosphor decay) is every 50000 cycles (30 Hz).
 * We render from the completed buffer (vectors_erse), not the in-progress one. */
#define EMU_CYCLES_PER_FRAME (VECTREX_MHZ / 60)

/* Snapshot buffer: copy vectors when vec2x signals a frame is complete */
static vector_t g_snap_vectors[VECTREX_MHZ / 30];  /* same size as vec2x internal buffer */
static long g_snap_count = 0;

void vec2x_platform_render(void) {
    /* Called by vec2x_emu right before buffer swap — vectors_draw is complete */
    g_snap_count = vector_draw_cnt;
    if (g_snap_count > 0)
        memcpy(g_snap_vectors, vectors_draw, g_snap_count * sizeof(vector_t));
}

static bool g_running = false;
static bool g_paused = false;

#define MAX_BP 16
static unsigned g_bp[MAX_BP];
static int g_bp_count = 0;
static bool g_bp_enabled = true;
static int g_last_bp_hit = 0;
static bool g_bp_skip_once = false;
static bool g_soft_bp_enabled = false;
static int g_last_soft_bp = 0;

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
    if (!g_running || g_paused) return;
    vec2x_emu(EMU_CYCLES_PER_FRAME);

    /* Check breakpoints */
    if (g_bp_enabled && g_bp_count > 0 && !g_bp_skip_once) {
        VemuCpuState st = vemu_get_cpu();
        for (int i = 0; i < g_bp_count; i++) {
            if (g_bp[i] == st.pc) {
                g_paused = true;
                g_last_bp_hit = (int)st.pc + 1; /* +1 so 0 means no hit */
                break;
            }
        }
    }
    g_bp_skip_once = false;

    /* Check software breakpoint */
    if (g_soft_bp_enabled && !g_paused) {
        unsigned char marker = e6809_read8(VEMU_SOFT_BP_ADDR);
        if (marker != 0) {
            /* Clear it */
            extern void (*e6809_write8)(unsigned address, unsigned char data);
            e6809_write8(VEMU_SOFT_BP_ADDR, 0);
            g_last_soft_bp = marker;
            g_paused = true;
        }
    }
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

    /* Render from snapshot taken at frame completion by vec2x_platform_render() */
    for (long v = 0; v < g_snap_count; v++) {
        unsigned char c = g_snap_vectors[v].color;
        if (c >= VECTREX_COLORS) continue;
        Uint8 intensity = (Uint8)(c * 255 / VECTREX_COLORS);
        SDL_SetRenderDrawColor(renderer, intensity, intensity, intensity, 255);

        int x0 = off_x + (int)(g_snap_vectors[v].x0 * scl);
        int y0 = off_y + (int)(g_snap_vectors[v].y0 * scl);
        int x1 = off_x + (int)(g_snap_vectors[v].x1 * scl);
        int y1 = off_y + (int)(g_snap_vectors[v].y1 * scl);
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

unsigned char vemu_read8(unsigned addr) {
    if (!g_running) return 0;
    return e6809_read8(addr);
}

void vemu_read_mem(unsigned addr, unsigned char *buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = g_running ? e6809_read8((addr + i) & 0xFFFF) : 0;
}

VemuCpuState vemu_get_cpu(void) {
    VemuCpuState st;
    e6809_get_regs(&st.pc, &st.a, &st.b, &st.x, &st.y, &st.u, &st.s, &st.dp, &st.cc);
    return st;
}

long vemu_get_vector_count(void) { return vector_draw_cnt; }

bool vemu_is_paused(void) { return g_paused; }
void vemu_pause(void) { g_paused = true; }
void vemu_resume(void) { g_paused = false; g_bp_skip_once = true; }

bool vemu_bp_enabled(void) { return g_bp_enabled; }
void vemu_set_bp_enabled(bool en) { g_bp_enabled = en; }

void vemu_add_breakpoint(unsigned addr) {
    if (g_bp_count < MAX_BP) g_bp[g_bp_count++] = addr;
}

void vemu_remove_breakpoint(unsigned addr) {
    for (int i = 0; i < g_bp_count; i++) {
        if (g_bp[i] == addr) {
            for (int j = i; j < g_bp_count - 1; j++) g_bp[j] = g_bp[j+1];
            g_bp_count--;
            return;
        }
    }
}

void vemu_clear_breakpoints(void) { g_bp_count = 0; }

int vemu_list_breakpoints(unsigned *out, int max) {
    int n = g_bp_count < max ? g_bp_count : max;
    for (int i = 0; i < n; i++) out[i] = g_bp[i];
    return n;
}

int vemu_get_last_bp_hit(void) { int v = g_last_bp_hit; g_last_bp_hit = 0; return v; }

void vemu_set_soft_bp_enabled(bool en) {
    g_soft_bp_enabled = en;
    if (en && g_running) {
        extern void (*e6809_write8)(unsigned address, unsigned char data);
        e6809_write8(VEMU_SOFT_BP_ADDR, 0);
    }
}
bool vemu_get_soft_bp_enabled(void) { return g_soft_bp_enabled; }
int vemu_get_last_soft_bp(void) { int v = g_last_soft_bp; g_last_soft_bp = 0; return v; }
