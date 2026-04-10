/* panel_gmem.c — GB memory viewer */
#include "app.h"
#include "ui.h"
#include "gbc_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char mem_buf[8] = "0000";
static bool mem_focus = false;

void draw_gmem(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Memory", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    if (!gbc_emu_is_running()) { ui_text_color(c.x, y, "Not running.", ui_theme.text_dim); ui_panel_end(); return; }
    ui_text_color(c.x, y+3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x+42, y, 70, mem_buf, sizeof(mem_buf), &mem_focus);
    y += ui_line_height() + 8;
    uint16_t addr = (uint16_t)strtoul(mem_buf, NULL, 16);
    int rows = (c.h - (y - c.y)) / (ui_line_height() + 1);
    if (rows > 32) rows = 32;
    for (int r = 0; r < rows; r++) {
        uint16_t ra = (addr + r * 8) & 0xFFFF;
        uint8_t data[8]; gbc_emu_read_mem(ra, data, 8);
        char line[64]; int p = snprintf(line, sizeof(line), "%04X: ", ra);
        char ascii[10];
        for (int i = 0; i < 8; i++) { p += snprintf(line+p, sizeof(line)-p, "%02X ", data[i]); ascii[i] = (data[i]>=32&&data[i]<127)?data[i]:'.'; }
        ascii[8]=0; snprintf(line+p, sizeof(line)-p, " %s", ascii);
        ui_text_mono(c.x, y, line); y += ui_line_height() + 1;
    }
    ui_panel_end();
}
