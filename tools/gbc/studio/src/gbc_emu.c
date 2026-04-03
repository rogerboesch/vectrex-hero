/*
 * gbc_emu.c — igb wrapper for embedded use in GBC Studio
 */

#include "gbc_emu.h"
#include "igb/gb.h"
#include <string.h>
#include <stdlib.h>

#define LCD_W GB_LCD_WIDTH   /* 160 */
#define LCD_H GB_LCD_HEIGHT  /* 144 */

static struct gb *g_gb = NULL;
static SDL_Texture *g_texture = NULL;
static uint32_t g_pixels[LCD_W * LCD_H];
static bool g_running = false;
static SDL_Renderer *g_renderer = NULL;

/* ── Frontend callbacks ───────────────────────────────────── */

static void fe_draw_line_dmg(struct gb *gb, unsigned ly,
                              union gb_gpu_color col[LCD_W]) {
    static const uint32_t dm[4] = {0xFF75A32C, 0xFF387A21, 0xFF255116, 0xFF12280B};
    (void)gb;
    for (unsigned i = 0; i < LCD_W; i++)
        g_pixels[ly * LCD_W + i] = dm[col[i].dmg_color];
}

static uint32_t gbc_to_argb(uint16_t c) {
    uint32_t r = ((c & 0x1F) << 3) | ((c & 0x1F) >> 2);
    uint32_t g = (((c >> 5) & 0x1F) << 3) | (((c >> 5) & 0x1F) >> 2);
    uint32_t b = (((c >> 10) & 0x1F) << 3) | (((c >> 10) & 0x1F) >> 2);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static void fe_draw_line_gbc(struct gb *gb, unsigned ly,
                              union gb_gpu_color col[LCD_W]) {
    (void)gb;
    for (unsigned i = 0; i < LCD_W; i++)
        g_pixels[ly * LCD_W + i] = gbc_to_argb(col[i].gbc_color);
}

static void fe_flip(struct gb *gb) {
    (void)gb;
    if (g_texture)
        SDL_UpdateTexture(g_texture, NULL, g_pixels, LCD_W * 4);
}

static void fe_refresh_input(struct gb *gb) { (void)gb; }
static void fe_destroy(struct gb *gb) { (void)gb; }

/* ── Public API ───────────────────────────────────────────── */

bool gbc_emu_load(SDL_Renderer *renderer, const char *rom_path) {
    if (g_gb) gbc_emu_stop();

    g_gb = calloc(1, sizeof(struct gb));
    if (!g_gb) return false;

    g_renderer = renderer;

    /* Init semaphores for SPU */
    for (unsigned i = 0; i < GB_SPU_SAMPLE_BUFFER_COUNT; i++) {
        struct gb_spu_sample_buffer *buf = &g_gb->spu.buffers[i];
        memset(buf->samples, 0, sizeof(buf->samples));
        sem_init(&buf->free, 0, 0);
        sem_init(&buf->ready, 0, 1);
    }

    /* Set frontend callbacks */
    g_gb->frontend.draw_line_dmg = fe_draw_line_dmg;
    g_gb->frontend.draw_line_gbc = fe_draw_line_gbc;
    g_gb->frontend.flip = fe_flip;
    g_gb->frontend.refresh_input = fe_refresh_input;
    g_gb->frontend.destroy = fe_destroy;

    /* Load ROM */
    gb_cart_load(g_gb, rom_path);

    /* Reset all subsystems */
    gb_sync_reset(g_gb);
    gb_irq_reset(g_gb);
    gb_cpu_reset(g_gb);
    gb_gpu_reset(g_gb);
    gb_input_reset(g_gb);
    gb_dma_reset(g_gb);
    gb_timer_reset(g_gb);
    gb_spu_reset(g_gb);

    g_gb->iram_high_bank = 1;
    g_gb->vram_high_bank = false;
    g_gb->quit = false;
    g_gb->double_speed = false;
    g_gb->speed_switch_pending = false;

    /* Create texture */
    if (!g_texture)
        g_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H);

    memset(g_pixels, 0, sizeof(g_pixels));
    g_running = true;
    return true;
}

void gbc_emu_stop(void) {
    if (g_gb) {
        gb_cart_unload(g_gb);
        free(g_gb);
        g_gb = NULL;
    }
    if (g_texture) { SDL_DestroyTexture(g_texture); g_texture = NULL; }
    g_running = false;
}

bool gbc_emu_is_running(void) { return g_running; }

void gbc_emu_step(void) {
    if (!g_running || !g_gb) return;
    /* Run ~1/120s worth of cycles */
    gb_cpu_run_cycles(g_gb, GB_CPU_FREQ_HZ / 120);
}

void gbc_emu_render(SDL_Renderer *renderer, int x, int y, int w, int h) {
    if (!g_texture) return;
    /* Scale to fit, maintain aspect ratio */
    float sx = (float)w / LCD_W;
    float sy = (float)h / LCD_H;
    float s = sx < sy ? sx : sy;
    int dw = (int)(LCD_W * s), dh = (int)(LCD_H * s);
    int dx = x + (w - dw) / 2, dy = y + (h - dh) / 2;
    SDL_Rect dst = {dx, dy, dw, dh};
    SDL_RenderCopy(renderer, g_texture, NULL, &dst);
}

/* ── Input ────────────────────────────────────────────────── */

void gbc_emu_key_down(SDL_Keycode key) {
    if (!g_gb) return;
    switch (key) {
    case SDLK_RIGHT: gb_input_set(g_gb, GB_INPUT_RIGHT, true); break;
    case SDLK_LEFT:  gb_input_set(g_gb, GB_INPUT_LEFT, true); break;
    case SDLK_UP:    gb_input_set(g_gb, GB_INPUT_UP, true); break;
    case SDLK_DOWN:  gb_input_set(g_gb, GB_INPUT_DOWN, true); break;
    case SDLK_a:     gb_input_set(g_gb, GB_INPUT_A, true); break;
    case SDLK_s:     gb_input_set(g_gb, GB_INPUT_B, true); break;
    case SDLK_RETURN: gb_input_set(g_gb, GB_INPUT_START, true); break;
    case SDLK_BACKSPACE: gb_input_set(g_gb, GB_INPUT_SELECT, true); break;
    default: break;
    }
}

void gbc_emu_key_up(SDL_Keycode key) {
    if (!g_gb) return;
    switch (key) {
    case SDLK_RIGHT: gb_input_set(g_gb, GB_INPUT_RIGHT, false); break;
    case SDLK_LEFT:  gb_input_set(g_gb, GB_INPUT_LEFT, false); break;
    case SDLK_UP:    gb_input_set(g_gb, GB_INPUT_UP, false); break;
    case SDLK_DOWN:  gb_input_set(g_gb, GB_INPUT_DOWN, false); break;
    case SDLK_a:     gb_input_set(g_gb, GB_INPUT_A, false); break;
    case SDLK_s:     gb_input_set(g_gb, GB_INPUT_B, false); break;
    case SDLK_RETURN: gb_input_set(g_gb, GB_INPUT_START, false); break;
    case SDLK_BACKSPACE: gb_input_set(g_gb, GB_INPUT_SELECT, false); break;
    default: break;
    }
}

/* ── Debug ────────────────────────────────────────────────── */

GbcCpuState gbc_emu_get_cpu(void) {
    GbcCpuState st = {0};
    if (!g_gb) return st;
    st.pc = g_gb->cpu.pc;
    st.sp = g_gb->cpu.sp;
    uint8_t f = (g_gb->cpu.f_z ? 0x80 : 0) | (g_gb->cpu.f_n ? 0x40 : 0) |
                (g_gb->cpu.f_h ? 0x20 : 0) | (g_gb->cpu.f_c ? 0x10 : 0);
    st.af = ((uint16_t)g_gb->cpu.a << 8) | f;
    st.bc = ((uint16_t)g_gb->cpu.b << 8) | g_gb->cpu.c;
    st.de = ((uint16_t)g_gb->cpu.d << 8) | g_gb->cpu.e;
    st.hl = ((uint16_t)g_gb->cpu.h << 8) | g_gb->cpu.l;
    return st;
}

uint8_t gbc_emu_read8(uint16_t addr) {
    if (!g_gb) return 0;
    return gb_memory_readb(g_gb, addr);
}
