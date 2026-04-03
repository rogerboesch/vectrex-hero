/* panel_gbp.c — GB breakpoints */
#include "app.h"
#include "ui.h"
#include "gbc_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char bp_buf[8] = "";
static bool bp_focus = false;
static int bp_sel = -1;

void draw_gbp(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Breakpoints", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    bool en = gbc_emu_get_bp_enabled();
    if (ui_checkbox(c.x, y, "Breakpoints", &en)) gbc_emu_set_bp_enabled(en);
    y += ui_line_height() + 8;
    ui_text_color(c.x, y+3, "Addr:", ui_theme.text_dim);
    ui_input_text(c.x+42, y, c.w-42-44, bp_buf, sizeof(bp_buf), &bp_focus);
    if (ui_button(c.x+c.w-40, y, 40, ui_line_height()+6, "Add")) {
        uint16_t a = (uint16_t)strtoul(bp_buf, NULL, 16);
        if (a > 0) { gbc_emu_add_bp(a); bp_buf[0] = 0; }
    }
    y += ui_line_height() + 10;
    uint16_t bps[16]; int cnt = gbc_emu_list_bps(bps, 16);
    for (int i = 0; i < cnt; i++) {
        char label[16]; snprintf(label, sizeof(label), "$%04X", bps[i]);
        if (ui_selectable(c.x, y, c.w, label, bp_sel == i)) bp_sel = i;
        y += ui_line_height() + 2;
        if (y > c.y + c.h - ui_line_height() * 3) break;
    }
    if (cnt == 0) ui_text_color(c.x, y, "No breakpoints.", ui_theme.text_dim);
    y = c.y + c.h - ui_line_height() - 8;
    int bw = (c.w - 4) / 2;
    if (ui_button(c.x, y, bw, ui_line_height()+4, "Remove")) {
        if (bp_sel >= 0 && bp_sel < cnt) { gbc_emu_remove_bp(bps[bp_sel]); bp_sel = -1; }
    }
    if (ui_button(c.x+bw+4, y, bw, ui_line_height()+4, "Clear All")) { gbc_emu_clear_bps(); bp_sel = -1; }
    ui_panel_end();
}
