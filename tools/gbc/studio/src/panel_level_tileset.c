/*
 * panel_level_tileset.c — Tileset selector (left panel for level editor)
 *
 * Shows all tileset entries as 8x8 tiles. Click to select current paint tile.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_level_tileset(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Tileset", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    Tileset *ts = &app->tmap.tileset;
    GBCPalette *pal = &app->tmap.bg_pals[0];

    int scale = 3;
    int tile_px = 8 * scale;  /* 24px per tile */
    int cols = c.w / (tile_px + 2);
    if (cols < 1) cols = 1;

    int y = c.y;
    for (int i = 0; i < ts->used_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int tx = c.x + col * (tile_px + 2);
        int ty = c.y + row * (tile_px + 2);

        if (ty > c.y + c.h) break;

        /* Draw tile */
        for (int py2 = 0; py2 < 8; py2++) {
            for (int px2 = 0; px2 < 8; px2++) {
                uint8_t ci = tset_get_pixel(&ts->entries[i], px2, py2);
                RGB5 rgb = pal->colors[ci];
                uint8_t r, g, b;
                rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect pr = {tx + px2 * scale, ty + py2 * scale, scale, scale};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &pr);
            }
        }

        /* Selection border */
        if (app->cur_tset_tile == i) {
            SDL_SetRenderDrawColor(app->renderer, 255, 255, 0, 255);
            SDL_Rect sr = {tx - 1, ty - 1, tile_px + 2, tile_px + 2};
            SDL_RenderDrawRect(app->renderer, &sr);
        }

        /* Click to select */
        if (ui_mouse_in_rect(tx, ty, tile_px, tile_px) && ui_mouse_clicked())
            app->cur_tset_tile = i;
    }

    /* Info at bottom */
    char info[32];
    snprintf(info, sizeof(info), "Tile: %d / %d", app->cur_tset_tile, ts->used_count);
    int iy = c.y + c.h - ui_line_height();
    ui_text_color(c.x, iy, info, ui_theme.text_dim);

    ui_panel_end();
}
