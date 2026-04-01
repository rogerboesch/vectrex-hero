/*
 * emulator.h — iQL emulator wrapper for QL Studio
 *
 * Runs the Sinclair QL emulator in a background thread.
 * Provides framebuffer access, keyboard input, memory access,
 * breakpoints, and CPU state inspection.
 */
#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>

class Emulator {
public:
    Emulator();
    ~Emulator();

    // Lifecycle
    bool start(const char *system_path);
    void stop();
    void restart();
    bool is_running() const { return running; }

    // Pause/resume
    void pause();
    void resume();
    bool is_paused() const { return paused; }

    // Display
    void update_texture();          // call from render loop
    GLuint get_texture() const { return fb_texture; }
    int get_screen_width() const;
    int get_screen_height() const;

    // Keyboard
    void send_key(int vk_code, bool pressed, bool shift = false,
                  bool ctrl = false, bool alt = false);

    // Speed
    void set_speed(int ms);  // 0=fast, 1=normal, 4=slow, 10=very slow

    // CPU state
    struct CpuState {
        uint32_t d[8], a[8];
        uint32_t pc, usp, ssp;
        uint16_t sr;
    };
    CpuState get_cpu_state() const;

    // Memory access
    uint8_t  read_byte(uint32_t addr);
    uint16_t read_word(uint32_t addr);
    uint32_t read_long(uint32_t addr);
    void read_mem(uint32_t addr, uint8_t *buf, int len);
    void write_byte(uint32_t addr, uint8_t val);
    void write_word(uint32_t addr, uint16_t val);
    void write_long(uint32_t addr, uint32_t val);

    // Single step
    void step(int count = 1);

    // Software breakpoints
    void set_soft_bp_enabled(bool en);
    bool get_soft_bp_enabled() const { return soft_bp_enabled; }
    int  check_soft_bp();  // returns ID (1-255) or 0 if none hit

    // PC breakpoints
    void add_breakpoint(uint32_t addr);
    void remove_breakpoint(uint32_t addr);
    void clear_breakpoints();
    std::vector<uint32_t> list_breakpoints() const;

    // Log callback
    using LogFunc = std::function<void(const char *)>;
    void set_log_callback(LogFunc cb) { log_cb = cb; }

    // Trap logging
    void set_trap_logging(bool en);
    bool get_trap_logging() const { return trap_logging; }
    std::string drain_trap_log();

    // Build
    bool build(const std::string &ql_dir, std::string &output);

private:
    bool running = false;
    bool paused = false;
    bool soft_bp_enabled = false;
    bool trap_logging = false;
    GLuint fb_texture = 0;
    SDL_Thread *emu_thread = nullptr;
    LogFunc log_cb;

    static int emu_thread_func(void *data);
};

// Global emulator instance (singleton — iQL uses global state)
extern Emulator g_emu;
