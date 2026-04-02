/*
 * panel_sprite_canvas.c — Pixel grid canvas with toolbar
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_sprite_canvas(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    if (app->current_sprite >= app->sprite_count) { ui_panel_end(); return; }
    Sprite *spr = &app->sprites[app->current_sprite];

    /* Toolbar buttons */
    int bx = tb.x;
    int bh = tb.h;
    int bw = 50;
    int gap = 4;

    if (ui_button(bx, tb.y, bw, bh, "L"))    { sprite_move_left(spr); }  bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "R"))    { sprite_move_right(spr); } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "U"))    { sprite_move_up(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "D"))    { sprite_move_down(spr); }  bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpH")) { sprite_flip_h(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpV")) { sprite_flip_v(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "Clear")){ sprite_clear(spr); }      bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "Copy")) {
        app->clipboard = *spr;
        app->has_clipboard = 1;
    }
    bx += bw + gap;
    if (app->has_clipboard) {
        if (ui_button(bx, tb.y, bw, bh, "Paste")) {
            memcpy(spr->pixels, app->clipboard.pixels, sizeof(spr->pixels));
            sprite_resize(spr, app->clipboard.width, app->clipboard.height);
        }
    }

    /* Calculate cell size to fit */
    float cell_x = (float)c.w / spr->width;
    float cell_y = (float)c.h / spr->height;
    float cell = cell_x < cell_y ? cell_x : cell_y;
    if (cell < 2) cell = 2;
    if (cell > 32) cell = 32;

    int total_w = (int)(spr->width * cell);
    int total_h = (int)(spr->height * cell);
    int ox = c.x + (c.w - total_w) / 2;
    int oy = c.y + (c.h - total_h) / 2;

    /* Draw pixels */
    for (int y = 0; y < spr->height; y++) {
        for (int x = 0; x < spr->width; x++) {
            SDL_Color col = QL_SDL_COLORS[spr->pixels[y][x]];
            int rx = ox + (int)(x * cell);
            int ry = oy + (int)(y * cell);
            int rw = (int)((x + 1) * cell) - (int)(x * cell);
            int rh = (int)((y + 1) * cell) - (int)(y * cell);

            SDL_Rect r = {rx, ry, rw, rh};
            SDL_SetRenderDrawColor(app->renderer, col.r, col.g, col.b, 255);
            SDL_RenderFillRect(app->renderer, &r);

            /* Grid */
            SDL_SetRenderDrawColor(app->renderer, ui_theme.canvas_grid.r, ui_theme.canvas_grid.g, ui_theme.canvas_grid.b, 255);
            SDL_RenderDrawRect(app->renderer, &r);
        }
    }

    /* Byte boundary lines (every 2 pixels) */
    SDL_SetRenderDrawColor(app->renderer, ui_theme.canvas_byte.r, ui_theme.canvas_byte.g, ui_theme.canvas_byte.b, 255);
    for (int x = 0; x <= spr->width; x += 2) {
        int lx = ox + (int)(x * cell);
        SDL_RenderDrawLine(app->renderer, lx, oy, lx, oy + total_h);
    }

    /* Mouse interaction */
    if (ui_mouse_in_rect(ox, oy, total_w, total_h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int px_x = (int)((mx - ox) / cell);
        int px_y = (int)((my - oy) / cell);
        if (px_x >= 0 && px_x < spr->width && px_y >= 0 && px_y < spr->height) {
            /* Left click: paint */
            if (ui_mouse_down()) {
                spr->pixels[px_y][px_x] = app->current_color;
                app->modified = 1;
            }
            /* Right click: pick color */
            if (ui_mouse_right_clicked()) {
                app->current_color = spr->pixels[px_y][px_x];
            }
            /* Tooltip */
            char tip[64];
            snprintf(tip, sizeof(tip), "(%d, %d) %d: %s", px_x, px_y,
                     spr->pixels[px_y][px_x], QL_COLOR_NAMES[spr->pixels[px_y][px_x]]);
            ui_tooltip(tip);
        }
    }

    ui_panel_end();
}
