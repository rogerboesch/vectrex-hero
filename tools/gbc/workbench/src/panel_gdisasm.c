/* panel_gdisasm.c — GB disassembly */
#include "app.h"
#include "ui.h"
#include "gbc_emu.h"
#include "gb_disasm.h"
#include <stdio.h>

static uint8_t dis_read(uint16_t addr) { return gbc_emu_read8(addr); }

void draw_gdisasm(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Disassembly", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    if (!gbc_emu_is_running()) { ui_text_color(c.x, y, "Not running.", ui_theme.text_dim); ui_panel_end(); return; }
    GbcCpuState cpu = gbc_emu_get_cpu();
    uint16_t addr = cpu.pc;
    int max = c.h / (ui_line_height() + 1);
    for (int i = 0; i < max && y < c.y + c.h - ui_line_height(); i++) {
        char mn[48]; int sz = gb_disasm(addr, mn, sizeof(mn), dis_read);
        char line[64]; snprintf(line, sizeof(line), "%04X  %s", addr, mn);
        SDL_Color col = ui_theme.text;
        if (addr == cpu.pc) {
            SDL_Rect hl = {c.x, y, c.w, ui_line_height()};
            SDL_SetRenderDrawColor(app->renderer, 45, 70, 100, 255);
            SDL_RenderFillRect(app->renderer, &hl);
            col = (SDL_Color){255,255,100,255};
        }
        ui_text_mono_color(c.x, y, line, col);
        y += ui_line_height() + 1; addr += sz;
    }
    ui_panel_end();
}
