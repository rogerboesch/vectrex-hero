/*
 * panel_breakpoints.c — Breakpoint list with add/remove/clear
 */

#include "app.h"
#include "ui.h"
#include "emulator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char bp_addr_buf[16] = "";
static bool bp_addr_focus = false;
static int bp_selected = -1;

void draw_breakpoints(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Breakpoints", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* Add address input + buttons */
    ui_text_color(c.x, y + 3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x + 42, y, c.w - 42 - 50, bp_addr_buf, sizeof(bp_addr_buf), &bp_addr_focus);
    if (ui_button(c.x + c.w - 44, y, 44, ui_line_height() + 6, "Add")) {
        uint32_t a = (uint32_t)strtoul(bp_addr_buf, NULL, 16);
        if (a > 0) {
            emu_add_breakpoint(a);
            bp_addr_buf[0] = 0;
        }
    }
    y += ui_line_height() + 12;

    /* Breakpoint list */
    uint32_t bps[16];
    int count = emu_list_breakpoints(bps, 16);

    for (int i = 0; i < count; i++) {
        char label[32];
        snprintf(label, sizeof(label), "$%06X", bps[i]);
        if (ui_selectable(c.x, y, c.w, label, bp_selected == i)) {
            bp_selected = i;
        }
        y += ui_line_height() + 2;
        if (y > c.y + c.h - ui_line_height() * 3) break;
    }

    if (count == 0) {
        ui_text_color(c.x, y, "No breakpoints set.", ui_theme.text_dim);
        y += ui_line_height() + 4;
    }

    /* Remove / Clear buttons at bottom */
    y = c.y + c.h - ui_line_height() - 8;
    int bw = (c.w - 4) / 2;
    if (ui_button(c.x, y, bw, ui_line_height() + 4, "Remove")) {
        if (bp_selected >= 0 && bp_selected < count) {
            emu_remove_breakpoint(bps[bp_selected]);
            bp_selected = -1;
        }
    }
    if (ui_button(c.x + bw + 4, y, bw, ui_line_height() + 4, "Clear All")) {
        emu_clear_breakpoints();
        bp_selected = -1;
    }

    ui_panel_end();
}
