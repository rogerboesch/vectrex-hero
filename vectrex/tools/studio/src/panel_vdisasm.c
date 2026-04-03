/*
 * panel_vdisasm.c — 6809 disassembly view centered on current PC
 */

#include "app.h"
#include "ui.h"
#include "vectrex_emu.h"
#include "m6809_disasm.h"
#include <stdio.h>

static unsigned char disasm_read(unsigned addr) {
    return vemu_read8(addr);
}

void draw_vdisasm(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Disassembly", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!vemu_is_running()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end(); return;
    }

    VemuCpuState cpu = vemu_get_cpu();
    unsigned pc = cpu.pc;

    int max_lines = (c.h) / (ui_line_height() + 1);
    unsigned addr = pc;

    for (int i = 0; i < max_lines && y < c.y + c.h - ui_line_height(); i++) {
        char mnemonic[64];
        int size = m6809_disasm(addr, mnemonic, sizeof(mnemonic), disasm_read);

        char line[80];
        snprintf(line, sizeof(line), "%04X  %s", addr, mnemonic);

        SDL_Color col = ui_theme.text;
        if (addr == pc) {
            SDL_Rect hl = {c.x, y, c.w, ui_line_height()};
            SDL_SetRenderDrawColor(app->renderer, 45, 70, 100, 255);
            SDL_RenderFillRect(app->renderer, &hl);
            col = (SDL_Color){255, 255, 100, 255};
        }

        ui_text_mono_color(c.x, y, line, col);
        y += ui_line_height() + 1;
        addr += size;
    }

    ui_panel_end();
}
