/*
 * panel_tile_editor.c — 8x16 pixel editor with grid
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_tile_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar */
    int bx = tb.x, bh = tb.h, bw = 50, gap = 4;
    if (ui_button(bx, tb.y, bw, bh, "Clear")) {
        if (app->cur_tile < app->tile_count) { tile_clear(&app->tiles[app->cur_tile]); app->modified = true; }
    }
    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpH")) {
        if (app->cur_tile < app->tile_count) { tile_flip_h(&app->tiles[app->cur_tile]); app->modified = true; }
    }
    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpV")) {
        if (app->cur_tile < app->tile_count) { tile_flip_v(&app->tiles[app->cur_tile]); app->modified = true; }
    }

    if (app->cur_tile >= app->tile_count) { ui_panel_end(); return; }
    GBCTile *tile = &app->tiles[app->cur_tile];
    GBCPalette *pal = &app->tmap.spr_pals[tile->palette < MAX_SPR_PALS ? tile->palette : 0];

    /* Compute cell size — fit 8x16 in the canvas */
    int cell = c.h / TILE_H;
    if (cell > c.w / TILE_W) cell = c.w / TILE_W;
    if (cell < 4) cell = 4;
    if (cell > 32) cell = 32;

    int total_w = TILE_W * cell, total_h = TILE_H * cell;
    int ox = c.x + (c.w - total_w) / 2;
    int oy = c.y + (c.h - total_h) / 2;

    /* Black background */
    SDL_Rect bg = {ox - 1, oy - 1, total_w + 2, total_h + 2};
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    /* Draw pixels */
    for (int ty = 0; ty < TILE_H; ty++) {
        for (int tx = 0; tx < TILE_W; tx++) {
            uint8_t ci = tile_get_pixel(tile, tx, ty);
            RGB5 rgb = pal->colors[ci];
            uint8_t r, g, b;
            rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {ox + tx * cell, oy + ty * cell, cell, cell};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);

            /* Grid */
            SDL_SetRenderDrawColor(app->renderer, 40, 40, 40, 255);
            SDL_RenderDrawRect(app->renderer, &pr);
        }
    }

    /* 8x8 boundary line (between top and bottom halves) */
    SDL_SetRenderDrawColor(app->renderer, 80, 80, 120, 255);
    SDL_RenderDrawLine(app->renderer, ox, oy + 8 * cell, ox + total_w, oy + 8 * cell);

    /* Mouse painting */
    if (ui_mouse_in_rect(ox, oy, total_w, total_h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int tx = (mx - ox) / cell;
        int ty = (my - oy) / cell;
        if (tx >= 0 && tx < TILE_W && ty >= 0 && ty < TILE_H) {
            if (ui_mouse_down()) {
                tile_set_pixel(tile, tx, ty, app->cur_color);
                app->modified = true;
            }
            if (ui_mouse_right_clicked()) {
                app->cur_color = tile_get_pixel(tile, tx, ty);
            }
            char tip[32];
            snprintf(tip, sizeof(tip), "(%d,%d) color %d", tx, ty, tile_get_pixel(tile, tx, ty));
            ui_tooltip(tip);
        }
    }

    ui_panel_end();
}
