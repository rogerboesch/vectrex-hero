/*
 * panel_memory.c — Hex dump memory viewer
 */

#include "app.h"
#include "ui.h"
#include "emulator.h"
#include <stdio.h>
#include <string.h>

static char mem_addr_buf[16] = "20000";
static int mem_rows = 16;
static bool mem_addr_focus = false;

void draw_memory(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Memory", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (!emu_is_ready()) {
        ui_text_color(c.x, y, "Emulator not running.", ui_theme.text_dim);
        ui_panel_end();
        return;
    }

    /* Address input */
    ui_text_color(c.x, y + 3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x + 42, y, 80, mem_addr_buf, sizeof(mem_addr_buf), &mem_addr_focus);
    y += ui_line_height() + 8;

    uint32_t addr = (uint32_t)strtoul(mem_addr_buf, NULL, 16);

    /* Hex dump */
    int max_rows = (c.h - (y - c.y)) / (ui_line_height() + 1);
    if (max_rows > mem_rows) max_rows = mem_rows;
    if (max_rows > 64) max_rows = 64;

    for (int r = 0; r < max_rows; r++) {
        uint32_t ra = addr + r * 16;
        char line[96];
        int pos = snprintf(line, sizeof(line), "%06X: ", ra);

        uint8_t data[16];
        emu_read_mem(ra, data, 16);

        for (int i = 0; i < 16; i++)
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i]);

        /* ASCII */
        char ascii[20];
        for (int i = 0; i < 16; i++)
            ascii[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
        ascii[16] = 0;

        pos += snprintf(line + pos, sizeof(line) - pos, " %s", ascii);

        ui_text(c.x, y, line);
        y += ui_line_height() + 1;
    }

    ui_panel_end();
}
