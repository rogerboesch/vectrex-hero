/*
 * emulator.h — iQL emulator wrapper (pure C)
 *
 * Runs the Sinclair QL emulator in a background thread.
 * Provides framebuffer, keyboard, memory, CPU state, breakpoints.
 */
#pragma once

#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t d[8], a[8];
    uint32_t pc, usp, ssp;
    uint16_t sr;
} EmuCpuState;

/* Lifecycle */
bool emu_start(SDL_Renderer *renderer, const char *system_path);
void emu_stop(void);
void emu_restart(void);
bool emu_is_running(void);
bool emu_is_ready(void);

/* Pause/resume */
void emu_pause(void);
void emu_resume(void);
bool emu_is_paused(void);

/* Display — call from render loop */
void emu_update_texture(void);
SDL_Texture *emu_get_texture(void);
int emu_screen_width(void);
int emu_screen_height(void);

/* Keyboard */
void emu_send_key(int vk_code, bool pressed, bool shift, bool ctrl, bool alt);

/* Speed */
void emu_set_speed(int ms);

/* CPU state */
EmuCpuState emu_get_cpu_state(void);

/* Memory access */
uint8_t  emu_read_byte(uint32_t addr);
uint16_t emu_read_word(uint32_t addr);
uint32_t emu_read_long(uint32_t addr);
void     emu_read_mem(uint32_t addr, uint8_t *buf, int len);
void     emu_write_byte(uint32_t addr, uint8_t val);

/* Single step */
void emu_step(int count);

/* Software breakpoints */
void emu_set_soft_bp_enabled(bool en);
bool emu_get_soft_bp_enabled(void);
int  emu_check_soft_bp(void);

/* PC breakpoints */
void emu_add_breakpoint(uint32_t addr);
void emu_remove_breakpoint(uint32_t addr);
void emu_clear_breakpoints(void);
int  emu_list_breakpoints(uint32_t *out, int max);

/* Trap logging */
void emu_set_trap_logging(bool en);
bool emu_get_trap_logging(void);
int  emu_drain_trap_log(char *buf, int max_len);

/* iQL log buffer */
int emu_drain_iql_log(char *buf, int max_len);

/* Build */
bool emu_build(const char *ql_dir, char *output, int output_max);

/* Screenshot */
bool emu_save_screenshot(const char *path);
