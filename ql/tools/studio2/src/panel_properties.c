/*
 * panel_properties.c — Sprite name, width, height, size info
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_properties(App *app, int x, int y, int w, int *out_y) {
    if (app->current_sprite >= app->sprite_count) { *out_y = y; return; }
    Sprite *spr = &app->sprites[app->current_sprite];

    y = ui_section(x, y, w, "Properties");

    int cx = x + 4;
    int cw = w - 8;

    /* Calculate label width for alignment */
    int lw = ui_text_width("Name:");
    int tw_w = ui_text_width("Width:");
    int tw_h = ui_text_width("Height:");
    if (tw_w > lw) lw = tw_w;
    if (tw_h > lw) lw = tw_h;
    int field_x = cx + lw + 8;
    int field_w = cw - lw - 8;

    /* Name — only sync from sprite when not editing */
    ui_text_color(cx, y + 3, "Name:", ui_theme.text_dim);
    if (!app->name_focus) {
        strncpy(app->name_buf, spr->name, sizeof(app->name_buf) - 1);
    }
    if (ui_input_text(field_x, y, field_w, app->name_buf, sizeof(app->name_buf), &app->name_focus)) {
        strncpy(spr->name, app->name_buf, sizeof(spr->name) - 1);
    }
    y += ui_line_height() + 10;

    /* Width */
    int ww = spr->width;
    if (ui_input_int_ex(cx, y, cw, "Width:", lw, &ww, 2, 2, MAX_SPRITE_W)) {
        sprite_resize(spr, ww, spr->height);
    }
    y += ui_line_height() + 10;

    /* Height */
    int hh = spr->height;
    if (ui_input_int_ex(cx, y, cw, "Height:", lw, &hh, 1, 1, MAX_SPRITE_H)) {
        sprite_resize(spr, spr->width, hh);
    }
    y += ui_line_height() + 14;

    /* Size info — centered, dark */
    char info[32];
    snprintf(info, sizeof(info), "Size: %d bytes", (spr->width / 2) * spr->height);
    int iw = ui_text_width(info);
    ui_text_color(cx + (cw - iw) / 2, y, info, ui_theme.text_dark);
    y += ui_line_height() + 8;

    *out_y = y;
}
