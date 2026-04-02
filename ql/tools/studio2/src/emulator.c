/*
 * emulator.c — iQL emulator wrapper (pure C)
 */

#include "emulator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* iQL C API */
extern int QLStart(void);
extern void QLTimer(void);
extern void QLRestart(void);
extern void QLStop(void);
extern void QLSetSpeed(int ms);
extern void QLPause(void);
extern void QLResume(void);

/* Memory */
typedef int32_t w32;
typedef int16_t w16;
typedef int8_t w8;
typedef w32 aw32, rw32;
typedef w16 aw16, rw16;
typedef w8 aw8, rw8;
extern rw8 ReadByte(aw32 addr);
extern rw16 ReadWord(aw32 addr);
extern rw32 ReadLong(aw32 addr);
extern void WriteByte(aw32 addr, aw8 d);
extern void WriteWord(aw32 addr, aw16 d);
extern void WriteLong(aw32 addr, aw32 d);
extern void ExecuteChunk(long n);

/* CPU state */
extern w32 reg[16];
extern w32 *theROM;
extern void *pc;
extern w32 usp, ssp;
extern unsigned short GetSR(void);

/* Screen */
extern void *pixel_buffer;
typedef unsigned int uw32;
typedef struct {
    uw32 qm_lo, qm_hi, qm_len, linel;
    int yres, xres;
} screen_specs;
extern screen_specs qlscreen;
extern void QLRBUpdatePixelBuffer(void);

/* Keyboard */
#include "rb_virtual_keys.h"
extern void QLRBSendEvent(RBEvent evt);

/* Platform */
extern void iql_set_system_path(const char *path);
extern void dosignal(void);
extern int iql_drain_log(char *out, int max_len);

/* Perf counters (required by patched iQL) */
volatile unsigned long iql_perf_instructions = 0;
volatile int iql_perf_chunks = 0;

/* ── Trap log buffer ──────────────────────────────────────── */

#define TRAP_LOG_SIZE 4096
static char trap_log_buf[TRAP_LOG_SIZE];
static int trap_log_head = 0;
static int trap_log_tail = 0;
static bool g_trap_logging = false;

static const char *trap1_names[] = {
    "MT.INF","MT.CJOB","MT.JINF",NULL,"MT.RJOB","MT.FRJOB",
    "MT.FREE","MT.TRAPV","MT.SUSJB","MT.RELJB","MT.ACTIV",
    "MT.PRIOR","MT.ALLOC","MT.LNKFR","MT.ALRES","MT.RERES",
    "MT.DMODE","MT.IPCOM","MT.BAUD","MT.RCLCK","MT.SCLCK",
    "MT.ACLCK","MT.ALBAS","MT.REBAS","MT.LXINT","MT.RXINT",
    "MT.LPOLL","MT.RPOLL","MT.LSCHD","MT.RSCHD",
    "MT.LIOD","MT.RIOD","MT.LDD","MT.RDD"
};

void iql_trap_hook(int trap_num) {
    if (!g_trap_logging) return;
    char buf[128];
    int d0 = (int)reg[0];
    int d1 = (int)reg[1];
    int d3 = (int)reg[3];
    const char *name = NULL;
    if (trap_num == 1) {
        int fn = d0 & 0xFF;
        if (fn < (int)(sizeof(trap1_names)/sizeof(trap1_names[0])))
            name = trap1_names[fn];
        snprintf(buf, sizeof(buf), "TRAP #1  d0=$%02X %-12s d1=$%08X d3=$%04X\n",
                 fn, name ? name : "?", (unsigned)d1, d3 & 0xFFFF);
    } else {
        snprintf(buf, sizeof(buf), "TRAP #%d  d0=$%08X\n", trap_num, (unsigned)d0);
    }
    for (const char *p = buf; *p; p++) {
        trap_log_buf[trap_log_head] = *p;
        trap_log_head = (trap_log_head + 1) % TRAP_LOG_SIZE;
        if (trap_log_head == trap_log_tail)
            trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
}

/* ── State ────────────────────────────────────────────────── */

static bool g_running = false;
static bool g_paused = false;
static bool g_rom_ready = false;
static bool g_soft_bp_enabled = false;
static SDL_Thread *g_emu_thread = NULL;
static SDL_Texture *g_fb_texture = NULL;
static SDL_Renderer *g_renderer = NULL;

#define SOFT_BP_ADDR 0x3FFFE
#define MAX_BREAKPOINTS 16
static uint32_t bp_addrs[MAX_BREAKPOINTS];
static int bp_count = 0;

/* ── Thread ───────────────────────────────────────────────── */

static int emu_thread_func(void *data) {
    (void)data;
    QLStart();
    g_running = false;
    return 0;
}

/* ── Lifecycle ────────────────────────────────────────────── */

bool emu_start(SDL_Renderer *renderer, const char *system_path) {
    if (g_running) emu_stop();
    g_renderer = renderer;
    iql_set_system_path(system_path);
    g_running = true;
    g_paused = false;
    g_rom_ready = false;
    g_emu_thread = SDL_CreateThread(emu_thread_func, "iQL", NULL);
    if (!g_emu_thread) { g_running = false; return false; }
    if (!g_fb_texture) {
        g_fb_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STREAMING, 512, 256);
    }
    return true;
}

void emu_stop(void) {
    if (!g_running) return;
    QLStop();
    if (g_emu_thread) {
        SDL_WaitThread(g_emu_thread, NULL);
        g_emu_thread = NULL;
    }
    g_running = false;
    g_paused = false;
}

void emu_restart(void) { if (g_running) QLRestart(); }
bool emu_is_running(void) { return g_running; }
bool emu_is_ready(void) { return g_running && g_rom_ready; }
void emu_pause(void) { if (g_running && !g_paused) { QLPause(); g_paused = true; } }
void emu_resume(void) { if (g_running && g_paused) { QLResume(); g_paused = false; } }
bool emu_is_paused(void) { return g_paused; }

/* ── Display ──────────────────────────────────────────────── */

void emu_update_texture(void) {
    if (!g_running) return;

    /* Allocate pixel buffer if iQL hasn't */
    if (!pixel_buffer) {
        int w = qlscreen.xres, h = qlscreen.yres;
        if (w > 0 && h > 0) {
            pixel_buffer = malloc(w * h * 4);
            if (pixel_buffer) memset(pixel_buffer, 0, w * h * 4);
        }
        if (!pixel_buffer) return;
    }
    if (!theROM) return;
    g_rom_ready = true;

    int w = qlscreen.xres, h = qlscreen.yres;
    if (w <= 0 || h <= 0) return;

    dosignal();
    QLRBUpdatePixelBuffer();

    /* Recreate texture if size changed */
    if (g_fb_texture) {
        int tw, th;
        SDL_QueryTexture(g_fb_texture, NULL, NULL, &tw, &th);
        if (tw != w || th != h) {
            SDL_DestroyTexture(g_fb_texture);
            g_fb_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STREAMING, w, h);
        }
    }
    if (g_fb_texture)
        SDL_UpdateTexture(g_fb_texture, NULL, pixel_buffer, w * 4);

    /* Check software breakpoint */
    if (g_soft_bp_enabled && !g_paused) {
        uint8_t marker = (uint8_t)ReadByte(SOFT_BP_ADDR);
        if (marker != 0) {
            WriteByte(SOFT_BP_ADDR, 0);
            emu_pause();
        }
    }
}

SDL_Texture *emu_get_texture(void) { return g_fb_texture; }
int emu_screen_width(void) { return qlscreen.xres; }
int emu_screen_height(void) { return qlscreen.yres; }

/* ── Keyboard ─────────────────────────────────────────────── */

void emu_send_key(int vk_code, bool pressed, bool shift, bool ctrl, bool alt) {
    RBEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = pressed ? RBEVT_KeyPressed : RBEVT_KeyReleased;
    evt.code = (RBVirtualKey)vk_code;
    evt.ch = 0;
    evt.alt = alt ? 1 : 0;
    evt.control = ctrl ? 1 : 0;
    evt.shift = shift ? 1 : 0;
    QLRBSendEvent(evt);
}

/* ── Speed ────────────────────────────────────────────────── */

void emu_set_speed(int ms) { QLSetSpeed(ms); }

/* ── CPU state ────────────────────────────────────────────── */

EmuCpuState emu_get_cpu_state(void) {
    EmuCpuState s = {0};
    if (!g_rom_ready) return s;
    for (int i = 0; i < 8; i++) {
        s.d[i] = (uint32_t)reg[i];
        s.a[i] = (uint32_t)reg[8 + i];
    }
    s.pc = (uint32_t)((char *)pc - (char *)theROM);
    s.usp = (uint32_t)usp;
    s.ssp = (uint32_t)ssp;
    s.sr = GetSR();
    return s;
}

/* ── Memory ───────────────────────────────────────────────── */

uint8_t  emu_read_byte(uint32_t addr) { return g_rom_ready ? (uint8_t)ReadByte(addr) : 0; }
uint16_t emu_read_word(uint32_t addr) { return g_rom_ready ? (uint16_t)ReadWord(addr) : 0; }
uint32_t emu_read_long(uint32_t addr) { return g_rom_ready ? (uint32_t)ReadLong(addr) : 0; }
void emu_read_mem(uint32_t addr, uint8_t *buf, int len) {
    if (!g_rom_ready) { memset(buf, 0, len); return; }
    for (int i = 0; i < len; i++) buf[i] = (uint8_t)ReadByte(addr + i);
}
void emu_write_byte(uint32_t addr, uint8_t val) { if (g_rom_ready) WriteByte(addr, val); }

/* ── Step ─────────────────────────────────────────────────── */

void emu_step(int count) { if (g_running) ExecuteChunk(count); }

/* ── Software breakpoints ─────────────────────────────────── */

void emu_set_soft_bp_enabled(bool en) {
    g_soft_bp_enabled = en;
    if (en && g_running) WriteByte(SOFT_BP_ADDR, 0);
}
bool emu_get_soft_bp_enabled(void) { return g_soft_bp_enabled; }
int emu_check_soft_bp(void) {
    if (!g_running || !g_soft_bp_enabled) return 0;
    uint8_t m = (uint8_t)ReadByte(SOFT_BP_ADDR);
    if (m != 0) { WriteByte(SOFT_BP_ADDR, 0); return m; }
    return 0;
}

/* ── PC breakpoints ───────────────────────────────────────── */

void emu_add_breakpoint(uint32_t addr) {
    if (bp_count < MAX_BREAKPOINTS) bp_addrs[bp_count++] = addr;
}
void emu_remove_breakpoint(uint32_t addr) {
    for (int i = 0; i < bp_count; i++) {
        if (bp_addrs[i] == addr) {
            for (int j = i; j < bp_count - 1; j++) bp_addrs[j] = bp_addrs[j+1];
            bp_count--;
            return;
        }
    }
}
void emu_clear_breakpoints(void) { bp_count = 0; }
int emu_list_breakpoints(uint32_t *out, int max) {
    int n = bp_count < max ? bp_count : max;
    memcpy(out, bp_addrs, n * sizeof(uint32_t));
    return n;
}

/* ── Trap logging ─────────────────────────────────────────── */

void emu_set_trap_logging(bool en) { g_trap_logging = en; }
bool emu_get_trap_logging(void) { return g_trap_logging; }
int emu_drain_trap_log(char *buf, int max_len) {
    int n = 0;
    while (trap_log_tail != trap_log_head && n < max_len - 1) {
        buf[n++] = trap_log_buf[trap_log_tail];
        trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
    buf[n] = 0;
    return n;
}

/* ── iQL log ──────────────────────────────────────────────── */

int emu_drain_iql_log(char *buf, int max_len) {
    return iql_drain_log(buf, max_len);
}

/* ── Build ────────────────────────────────────────────────── */

bool emu_build(const char *ql_dir, char *output, int output_max) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && make 2>&1", ql_dir);
    FILE *fp = popen(cmd, "r");
    if (!fp) { snprintf(output, output_max, "Failed to run make"); return false; }
    int pos = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        int len = (int)strlen(buf);
        if (pos + len < output_max - 1) {
            memcpy(output + pos, buf, len);
            pos += len;
        }
    }
    output[pos] = 0;
    return pclose(fp) == 0;
}

/* ── Screenshot ───────────────────────────────────────────── */

bool emu_save_screenshot(const char *path) {
    if (!g_rom_ready || !pixel_buffer) return false;
    int w = qlscreen.xres, h = qlscreen.yres;
    if (w <= 0 || h <= 0) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    uint8_t *rgba = (uint8_t *)pixel_buffer;
    for (int i = 0; i < w * h; i++)
        fwrite(&rgba[i * 4], 1, 3, f);
    fclose(f);
    return true;
}
