/*
 * panel_vbp.c — Vectrex breakpoints panel
 */

#include "app.h"
#include "ui.h"
#include "vectrex_emu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char bp_addr_buf[8] = "";
static bool bp_addr_focus = false;
static int bp_selected = -1;

void draw_vbp(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Breakpoints", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* PC breakpoints toggle */
    bool en = vemu_bp_enabled();
    if (ui_checkbox(c.x, y, "PC Breakpoints", &en))
        vemu_set_bp_enabled(en);
    y += ui_line_height() + 4;

    /* Software breakpoints toggle */
    bool soft_en = vemu_get_soft_bp_enabled();
    if (ui_checkbox(c.x, y, "Software BPs", &soft_en))
        vemu_set_soft_bp_enabled(soft_en);
    y += ui_line_height() + 8;

    /* Address input + Add */
    ui_text_color(c.x, y + 3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x + 42, y, c.w - 42 - 44, bp_addr_buf, sizeof(bp_addr_buf), &bp_addr_focus);
    if (ui_button(c.x + c.w - 40, y, 40, ui_line_height() + 6, "Add")) {
        unsigned a = (unsigned)strtoul(bp_addr_buf, NULL, 16);
        if (a > 0) { vemu_add_breakpoint(a); bp_addr_buf[0] = 0; }
    }
    y += ui_line_height() + 10;

    /* List */
    unsigned bps[16];
    int count = vemu_list_breakpoints(bps, 16);

    for (int i = 0; i < count; i++) {
        char label[16];
        snprintf(label, sizeof(label), "$%04X", bps[i]);
        if (ui_selectable(c.x, y, c.w, label, bp_selected == i))
            bp_selected = i;
        y += ui_line_height() + 2;
        if (y > c.y + c.h - ui_line_height() * 3) break;
    }

    if (count == 0)
        ui_text_color(c.x, y, "No breakpoints.", ui_theme.text_dim);

    /* Remove / Clear at bottom */
    y = c.y + c.h - ui_line_height() - 8;
    int bw = (c.w - 4) / 2;
    if (ui_button(c.x, y, bw, ui_line_height() + 4, "Remove")) {
        if (bp_selected >= 0 && bp_selected < count) {
            vemu_remove_breakpoint(bps[bp_selected]);
            bp_selected = -1;
        }
    }
    if (ui_button(c.x + bw + 4, y, bw, ui_line_height() + 4, "Clear All")) {
        vemu_clear_breakpoints();
        bp_selected = -1;
    }

    ui_panel_end();
}
