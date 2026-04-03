/*
 * panel_vcpu.c — 6809 CPU state display
 */

#include "app.h"
#include "ui.h"
#include "vectrex_emu.h"
#include <stdio.h>

void draw_vcpu(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("CPU State", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!vemu_is_running()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end(); return;
    }

    VemuCpuState cpu = vemu_get_cpu();
    char buf[64];

    snprintf(buf, sizeof(buf), "PC: %04X", cpu.pc);
    ui_text_mono_color(c.x, y, buf, (SDL_Color){80, 200, 180, 255});
    y += ui_line_height() + 2;

    /* CC flags: E F H I N Z V C */
    snprintf(buf, sizeof(buf), "CC: %02X  %c%c%c%c%c%c%c%c",
             cpu.cc,
             (cpu.cc & 0x80) ? 'E' : '-', (cpu.cc & 0x40) ? 'F' : '-',
             (cpu.cc & 0x20) ? 'H' : '-', (cpu.cc & 0x10) ? 'I' : '-',
             (cpu.cc & 0x08) ? 'N' : '-', (cpu.cc & 0x04) ? 'Z' : '-',
             (cpu.cc & 0x02) ? 'V' : '-', (cpu.cc & 0x01) ? 'C' : '-');
    ui_text_mono_color(c.x, y, buf, ui_theme.text_dim);
    y += ui_line_height() + 6;

    snprintf(buf, sizeof(buf), "A: %02X    B: %02X", cpu.a, cpu.b);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 1;
    snprintf(buf, sizeof(buf), "D: %04X", (cpu.a << 8) | cpu.b);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 4;

    snprintf(buf, sizeof(buf), "X: %04X  Y: %04X", cpu.x, cpu.y);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 1;
    snprintf(buf, sizeof(buf), "U: %04X  S: %04X", cpu.u, cpu.s);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 1;
    snprintf(buf, sizeof(buf), "DP: %02X", cpu.dp);
    ui_text_mono(c.x, y, buf); y += ui_line_height() + 6;

    snprintf(buf, sizeof(buf), "Vectors: %ld", vemu_get_vector_count());
    ui_text_mono_color(c.x, y, buf, ui_theme.text_dim);

    ui_panel_end();
}
