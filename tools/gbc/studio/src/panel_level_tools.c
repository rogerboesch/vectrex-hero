/*
 * panel_level_tools.c — Level management tools (right panel)
 *
 * Add/remove/copy/prev/next level, clear, move (scroll controls).
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_level_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int bw = 30, gap = 3;

    /* Level browser */
    y = ui_section(c.x - 4, y, c.w + 8, "Level");

    TilemapLevel *lvl = (app->cur_level >= 0 && app->cur_level < app->tmap.level_count)
                        ? &app->tmap.levels[app->cur_level] : NULL;
    char label[48];
    snprintf(label, sizeof(label), "%s (%d / %d)",
             lvl ? lvl->name : "—", app->cur_level + 1, app->tmap.level_count);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    int bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_level > 0) app->cur_level--;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (app->cur_level < app->tmap.level_count - 1) app->cur_level++;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "+")) {
        if (app->tmap.level_count < TMAP_MAX_LEVELS) {
            TilemapLevel *nl = &app->tmap.levels[app->tmap.level_count];
            memset(nl, 0, sizeof(*nl));
            snprintf(nl->name, sizeof(nl->name), "Level %d", app->tmap.level_count + 1);
            nl->width = 20; nl->height = 16;
            app->cur_level = app->tmap.level_count;
            app->tmap.level_count++;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "-")) {
        if (app->tmap.level_count > 1) {
            for (int i = app->cur_level; i < app->tmap.level_count - 1; i++)
                app->tmap.levels[i] = app->tmap.levels[i + 1];
            app->tmap.level_count--;
            if (app->cur_level >= app->tmap.level_count) app->cur_level = app->tmap.level_count - 1;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, 40, ui_line_height() + 4, "Copy")) {
        if (app->tmap.level_count < TMAP_MAX_LEVELS && lvl) {
            app->tmap.levels[app->tmap.level_count] = *lvl;
            snprintf(app->tmap.levels[app->tmap.level_count].name, 32, "%s copy", lvl->name);
            app->cur_level = app->tmap.level_count;
            app->tmap.level_count++;
            app->modified = true;
        }
    }
    y += ui_line_height() + 12;

    /* Size */
    if (lvl) {
        y = ui_section(c.x - 4, y, c.w + 8, "Size");
        int lw = ui_text_width("Height:");
        if (ui_input_int_ex(c.x, y, c.w, "Width:", lw, &lvl->width, 1, 1, TMAP_MAX_W))
            app->modified = true;
        y += ui_line_height() + 8;
        if (ui_input_int_ex(c.x, y, c.w, "Height:", lw, &lvl->height, 1, 1, TMAP_MAX_H))
            app->modified = true;
        y += ui_line_height() + 12;
    }

    /* Actions */
    y = ui_section(c.x - 4, y, c.w + 8, "Actions");
    bw = (c.w - 4) / 2;
    if (ui_button(c.x, y, bw, ui_line_height() + 4, "Clear")) {
        if (lvl) { memset(lvl->tiles, 0, sizeof(lvl->tiles)); app->modified = true; }
    }
    y += ui_line_height() + 8;

    /* Scroll info */
    y = ui_section(c.x - 4, y, c.w + 8, "Scroll");
    snprintf(label, sizeof(label), "X: %d  Y: %d", app->scroll_x, app->scroll_y);
    ui_text_color(c.x, y, label, ui_theme.text_dim);
    y += ui_line_height() + 4;
    ui_text_small(c.x, y, "Arrow keys to scroll");

    ui_panel_end();
}
