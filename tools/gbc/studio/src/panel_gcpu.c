/* panel_gcpu.c — GB CPU state */
#include "app.h"
#include "ui.h"
#include "gbc_emu.h"
#include <stdio.h>

void draw_gcpu(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("CPU State", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    if (!gbc_emu_is_running()) { ui_text_color(c.x, y, "Not running.", ui_theme.text_dim); ui_panel_end(); return; }
    GbcCpuState cpu = gbc_emu_get_cpu();
    char buf[48];
    snprintf(buf, sizeof(buf), "PC:%04X  SP:%04X", cpu.pc, cpu.sp);
    ui_text_mono_color(c.x, y, buf, (SDL_Color){80,200,180,255}); y += ui_line_height() + 2;
    uint8_t f = cpu.af & 0xFF;
    snprintf(buf, sizeof(buf), "F: %c%c%c%c", f&0x80?'Z':'-', f&0x40?'N':'-', f&0x20?'H':'-', f&0x10?'C':'-');
    ui_text_mono_color(c.x, y, buf, ui_theme.text_dim); y += ui_line_height() + 6;
    snprintf(buf, sizeof(buf), "AF:%04X  BC:%04X", cpu.af, cpu.bc);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 1;
    snprintf(buf, sizeof(buf), "DE:%04X  HL:%04X", cpu.de, cpu.hl);
    ui_text_mono(c.x, y, buf);
    ui_panel_end();
}
