/*
 * panel_cpu_state.c — CPU registers, flags, step buttons
 */

#include "app.h"
#include "ui.h"
#include "emulator.h"
#include <stdio.h>

static const char *sr_flags(uint16_t sr) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%c%c%d%c%c%c%c%c",
        sr & 0x8000 ? 'T' : '-', sr & 0x2000 ? 'S' : '-',
        (sr >> 8) & 7,
        sr & 0x10 ? 'X' : '-', sr & 0x08 ? 'N' : '-',
        sr & 0x04 ? 'Z' : '-', sr & 0x02 ? 'V' : '-',
        sr & 0x01 ? 'C' : '-');
    return buf;
}

void draw_cpu_state(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("CPU State", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!emu_is_ready()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end();
        return;
    }

    EmuCpuState cpu = emu_get_cpu_state();
    char buf[64];

    snprintf(buf, sizeof(buf), "PC: $%06X  SR: $%04X", cpu.pc, cpu.sr);
    ui_text_color(c.x, y, buf, (SDL_Color){80, 200, 180, 255});
    y += ui_line_height() + 2;

    snprintf(buf, sizeof(buf), "Flags: %s", sr_flags(cpu.sr));
    ui_text_color(c.x, y, buf, ui_theme.text_dim);
    y += ui_line_height() + 6;

    /* Data registers */
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "D%d:%08X  D%d:%08X", i, cpu.d[i], i+4, cpu.d[i+4]);
        ui_text(c.x, y, buf);
        y += ui_line_height() + 1;
    }
    y += 4;

    /* Address registers */
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "A%d:%08X  A%d:%08X", i, cpu.a[i], i+4, cpu.a[i+4]);
        ui_text(c.x, y, buf);
        y += ui_line_height() + 1;
    }
    y += 4;

    snprintf(buf, sizeof(buf), "USP:%08X  SSP:%08X", cpu.usp, cpu.ssp);
    ui_text(c.x, y, buf);
    y += ui_line_height() + 8;

    /* Step buttons */
    int bw = 55, gap = 4;
    if (ui_button(c.x, y, bw, ui_line_height() + 4, "Step 1"))   { emu_pause(); emu_step(1); }
    if (ui_button(c.x + bw + gap, y, bw, ui_line_height() + 4, "Step 10"))  { emu_pause(); emu_step(10); }
    if (ui_button(c.x + (bw + gap) * 2, y, bw, ui_line_height() + 4, "Step100")) { emu_pause(); emu_step(100); }

    ui_panel_end();
}
