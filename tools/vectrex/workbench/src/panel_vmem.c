/*
 * panel_vmem.c — Vectrex memory hex dump viewer
 */

#include "app.h"
#include "ui.h"
#include "vectrex_emu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char mem_addr_buf[8] = "0000";
static bool mem_addr_focus = false;

void draw_vmem(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Memory", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!vemu_is_running()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end(); return;
    }

    /* Address input */
    ui_text_color(c.x, y + 3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x + 42, y, 70, mem_addr_buf, sizeof(mem_addr_buf), &mem_addr_focus);
    y += ui_line_height() + 8;

    unsigned addr = (unsigned)strtoul(mem_addr_buf, NULL, 16);

    /* Hex dump */
    int max_rows = (c.h - (y - c.y)) / (ui_line_height() + 1);
    if (max_rows > 32) max_rows = 32;

    for (int r = 0; r < max_rows; r++) {
        unsigned ra = (addr + r * 8) & 0xFFFF;
        unsigned char data[8];
        vemu_read_mem(ra, data, 8);

        char line[64];
        int pos = snprintf(line, sizeof(line), "%04X: ", ra);
        for (int i = 0; i < 8; i++)
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i]);

        /* ASCII */
        char ascii[10];
        for (int i = 0; i < 8; i++)
            ascii[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
        ascii[8] = 0;
        snprintf(line + pos, sizeof(line) - pos, " %s", ascii);

        ui_text_mono(c.x, y, line);
        y += ui_line_height() + 1;
    }

    ui_panel_end();
}
