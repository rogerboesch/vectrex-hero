/*
 * emulator.cpp — iQL emulator wrapper
 */

#include "emulator.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>

// iQL C API (from iQL emulator core)
extern "C" {
    // Emulator lifecycle
    extern int QLStart(void);
    extern void QLTimer(void);
    extern void QLRestart(void);
    extern void QLStop(void);
    extern void QLSetSpeed(int ms);
    extern void QLPause(void);
    extern void QLResume(void);

    // Memory access
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

    // CPU state
    extern w32 reg[16];
    extern w32 *theROM;
    extern void *pc;
    extern w32 usp, ssp;
    extern unsigned short GetSR(void);

    // Screen
    extern void *pixel_buffer;
    typedef struct { int xres, yres; long qm_lo, qm_len; } screen_specs;
    extern screen_specs qlscreen;
    extern void ql_render_screen(void *buffer);

    // Keyboard
    typedef struct { int type; int code; int shift; int control; int alt; } RBEvent;
    enum { RBEVT_KeyPressed = 1, RBEVT_KeyReleased = 2 };
    extern void QLRBSendEvent(RBEvent evt);

    // Platform stub
    extern void iql_set_system_path(const char *path);

    // Trap hook (defined in patched base_instructions_pz.c)
    // We provide the implementation here
    volatile unsigned long iql_perf_instructions = 0;
    volatile int iql_perf_chunks = 0;
}

// Trap log buffer
#define TRAP_LOG_SIZE 4096
static char trap_log_buf[TRAP_LOG_SIZE];
static int trap_log_head = 0;
static int trap_log_tail = 0;
static bool g_trap_logging = false;

// Breakpoint state
#define MAX_BREAKPOINTS 16
static uint32_t bp_addrs[MAX_BREAKPOINTS];
static int bp_count = 0;

// Software breakpoint
#define SOFT_BP_ADDR 0x3FFFE
static bool g_soft_bp_enabled = false;

// Trap names
static const char *trap1_names[] = {
    "MT.INF","MT.CJOB","MT.JINF",NULL,"MT.RJOB","MT.FRJOB",
    "MT.FREE","MT.TRAPV","MT.SUSJB","MT.RELJB","MT.ACTIV",
    "MT.PRIOR","MT.ALLOC","MT.LNKFR","MT.ALRES","MT.RERES",
    "MT.DMODE","MT.IPCOM","MT.BAUD","MT.RCLCK","MT.SCLCK",
    "MT.ACLCK","MT.ALBAS","MT.REBAS","MT.LXINT","MT.RXINT",
    "MT.LPOLL","MT.RPOLL","MT.LSCHD","MT.RSCHD",
    "MT.LIOD","MT.RIOD","MT.LDD","MT.RDD"
};

// Called from patched TRAP instruction handler (background thread)
extern "C" void iql_trap_hook(int trap_num) {
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

    // Append to circular buffer
    for (const char *p = buf; *p; p++) {
        trap_log_buf[trap_log_head] = *p;
        trap_log_head = (trap_log_head + 1) % TRAP_LOG_SIZE;
        if (trap_log_head == trap_log_tail)
            trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
}

// Global instance
Emulator g_emu;

Emulator::Emulator() {}

Emulator::~Emulator() {
    stop();
    if (fb_texture) glDeleteTextures(1, &fb_texture);
}

int Emulator::emu_thread_func(void *data) {
    (void)data;
    QLStart();
    g_emu.running = false;
    return 0;
}

bool Emulator::start(const char *system_path) {
    if (running) stop();

    iql_set_system_path(system_path);

    running = true;
    paused = false;
    emu_thread = SDL_CreateThread(emu_thread_func, "iQL", nullptr);
    if (!emu_thread) {
        running = false;
        return false;
    }

    if (!fb_texture) glGenTextures(1, &fb_texture);
    return true;
}

void Emulator::stop() {
    if (!running) return;
    QLStop();
    if (emu_thread) {
        SDL_WaitThread(emu_thread, nullptr);
        emu_thread = nullptr;
    }
    running = false;
    paused = false;
}

void Emulator::restart() {
    if (running) QLRestart();
}

void Emulator::pause() {
    if (running && !paused) {
        QLPause();
        paused = true;
    }
}

void Emulator::resume() {
    if (running && paused) {
        QLResume();
        paused = false;
    }
}

void Emulator::set_speed(int ms) {
    QLSetSpeed(ms);
}

int Emulator::get_screen_width() const {
    return qlscreen.xres;
}

int Emulator::get_screen_height() const {
    return qlscreen.yres;
}

void Emulator::update_texture() {
    if (!running || !pixel_buffer || !theROM) return;
    rom_ready = true;  // ROM is loaded once pixel_buffer and theROM are valid

    int w = qlscreen.xres;
    int h = qlscreen.yres;
    if (w <= 0 || h <= 0) return;

    glBindTexture(GL_TEXTURE_2D, fb_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixel_buffer);

    // Check software breakpoint
    if (soft_bp_enabled && !paused) {
        uint8_t marker = (uint8_t)ReadByte(SOFT_BP_ADDR);
        if (marker != 0) {
            WriteByte(SOFT_BP_ADDR, 0);
            pause();
            if (log_cb) {
                char msg[64];
                snprintf(msg, sizeof(msg), "*** SOFTWARE BP #%d ***", marker);
                log_cb(msg);
            }
        }
    }
}

void Emulator::send_key(int vk_code, bool pressed, bool shift, bool ctrl, bool alt) {
    RBEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = pressed ? RBEVT_KeyPressed : RBEVT_KeyReleased;
    evt.code = vk_code;
    evt.shift = shift ? 1 : 0;
    evt.control = ctrl ? 1 : 0;
    evt.alt = alt ? 1 : 0;
    QLRBSendEvent(evt);
}

Emulator::CpuState Emulator::get_cpu_state() const {
    CpuState s = {};
    if (!rom_ready) return s;
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

uint8_t  Emulator::read_byte(uint32_t addr) { if (!rom_ready) return 0; return (uint8_t)ReadByte(addr); }
uint16_t Emulator::read_word(uint32_t addr) { if (!rom_ready) return 0; return (uint16_t)ReadWord(addr); }
uint32_t Emulator::read_long(uint32_t addr) { if (!rom_ready) return 0; return (uint32_t)ReadLong(addr); }
void Emulator::read_mem(uint32_t addr, uint8_t *buf, int len) {
    if (!rom_ready) { memset(buf, 0, len); return; }
    for (int i = 0; i < len; i++) buf[i] = (uint8_t)ReadByte(addr + i);
}
void Emulator::write_byte(uint32_t addr, uint8_t val) { if (rom_ready) WriteByte(addr, val); }
void Emulator::write_word(uint32_t addr, uint16_t val) { if (rom_ready) WriteWord(addr, val); }
void Emulator::write_long(uint32_t addr, uint32_t val) { if (rom_ready) WriteLong(addr, val); }

void Emulator::step(int count) {
    if (!running) return;
    ExecuteChunk(count);
}

void Emulator::set_soft_bp_enabled(bool en) {
    soft_bp_enabled = en;
    if (en && running) WriteByte(SOFT_BP_ADDR, 0);
    g_soft_bp_enabled = en;
}

int Emulator::check_soft_bp() {
    if (!running || !soft_bp_enabled) return 0;
    uint8_t marker = (uint8_t)ReadByte(SOFT_BP_ADDR);
    if (marker != 0) {
        WriteByte(SOFT_BP_ADDR, 0);
        return marker;
    }
    return 0;
}

void Emulator::add_breakpoint(uint32_t addr) {
    if (bp_count < MAX_BREAKPOINTS)
        bp_addrs[bp_count++] = addr;
}

void Emulator::remove_breakpoint(uint32_t addr) {
    for (int i = 0; i < bp_count; i++) {
        if (bp_addrs[i] == addr) {
            for (int j = i; j < bp_count - 1; j++)
                bp_addrs[j] = bp_addrs[j + 1];
            bp_count--;
            return;
        }
    }
}

void Emulator::clear_breakpoints() { bp_count = 0; }

std::vector<uint32_t> Emulator::list_breakpoints() const {
    return std::vector<uint32_t>(bp_addrs, bp_addrs + bp_count);
}

void Emulator::set_trap_logging(bool en) {
    trap_logging = en;
    g_trap_logging = en;
}

std::string Emulator::drain_trap_log() {
    std::string out;
    while (trap_log_tail != trap_log_head) {
        out += trap_log_buf[trap_log_tail];
        trap_log_tail = (trap_log_tail + 1) % TRAP_LOG_SIZE;
    }
    return out;
}

bool Emulator::build(const std::string &ql_dir, std::string &output) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && make 2>&1", ql_dir.c_str());
    FILE *fp = popen(cmd, "r");
    if (!fp) { output = "Failed to run make"; return false; }

    output.clear();
    char buf[256];
    while (fgets(buf, sizeof(buf), fp))
        output += buf;

    int status = pclose(fp);
    return status == 0;
}
