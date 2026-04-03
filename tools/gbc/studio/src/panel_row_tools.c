/*
 * panel_row_tools.c — Row type browser + tools (left panel for Row Editor)
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_row_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Row Types", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* Row type browser */
    y = ui_section(c.x - 4, y, c.w + 8, "Row Type");

    char label[64];
    snprintf(label, sizeof(label), "%s (%d / %d)",
             app->level_project.row_types[app->cur_row_type].name,
             app->cur_row_type + 1, app->level_project.row_type_count);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    int bw = 30, gap = 3, bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_row_type > 0) app->cur_row_type--;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (app->cur_row_type < app->level_project.row_type_count - 1) app->cur_row_type++;
    }
    y += ui_line_height() + 12;

    /* Info */
    y = ui_section(c.x - 4, y, c.w + 8, "Info");
    RowType *rt = &app->level_project.row_types[app->cur_row_type];
    snprintf(label, sizeof(label), "Polylines: %d", rt->cave_line_count);
    ui_text_color(c.x, y, label, ui_theme.text_dim);
    y += ui_line_height() + 2;

    int total_pts = 0;
    for (int i = 0; i < rt->cave_line_count; i++)
        total_pts += rt->cave_lines[i].count;
    snprintf(label, sizeof(label), "Points: %d", total_pts);
    ui_text_color(c.x, y, label, ui_theme.text_dim);
    y += ui_line_height() + 12;

    /* Help */
    y = ui_section(c.x - 4, y, c.w + 8, "Help");
    ui_text_small(c.x, y, "Click: start drawing");
    y += ui_line_height();
    ui_text_small(c.x, y, "Right-click: finish");
    y += ui_line_height();
    ui_text_small(c.x, y, "Click point: select");
    y += ui_line_height();
    ui_text_small(c.x, y, "Drag: move point");
    y += ui_line_height();
    ui_text_small(c.x, y, "Delete: remove point");
    y += ui_line_height();
    ui_text_small(c.x, y, "Escape: cancel draw");

    ui_panel_end();
}
