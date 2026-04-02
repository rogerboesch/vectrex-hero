/*
 * panel_disasm.c — 68000 disassembly view centered on current PC
 */

#include "app.h"
#include "ui.h"
#include "emulator.h"
#include "m68k_disasm.h"
#include <stdio.h>

static uint16_t disasm_read_word(uint32_t addr) {
    return emu_read_word(addr);
}

void draw_disasm(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Disassembly", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!emu_is_ready()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end();
        return;
    }

    EmuCpuState cpu = emu_get_cpu_state();
    uint32_t pc = cpu.pc;

    /* Disassemble a few instructions before PC for context */
    /* Since 68K is variable length, we can't go backwards reliably.
       Instead, start from PC and show forward. */

    int max_lines = (c.h) / (ui_line_height() + 1);
    uint32_t addr = pc;

    for (int i = 0; i < max_lines && y < c.y + c.h - ui_line_height(); i++) {
        char mnemonic[64];
        int size = m68k_disasm(addr, mnemonic, sizeof(mnemonic), disasm_read_word);

        char line[96];
        snprintf(line, sizeof(line), "%06X  %s", addr, mnemonic);

        /* Highlight current PC */
        SDL_Color col = ui_theme.text;
        if (addr == pc) {
            /* Draw highlight bar */
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
