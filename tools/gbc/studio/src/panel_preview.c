/*
 * panel_preview.c — Preview all sprites at actual GBC size
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_preview(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Sprite Preview", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int scale = 4;
    int gap = 12;
    int x = c.x + 10, y = c.y + 10;

    for (int i = 0; i < app->tile_count; i++) {
        GBCTile *tile = &app->tiles[i];
        GBCPalette *pal = &app->spr_pals[tile->palette < MAX_SPR_PALS ? tile->palette : 0];

        /* Black background */
        SDL_Rect bg = {x - 1, y - 1, TILE_W * scale + 2, TILE_H * scale + 2};
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(app->renderer, &bg);

        /* Draw tile */
        for (int ty = 0; ty < TILE_H; ty++) {
            for (int tx = 0; tx < TILE_W; tx++) {
                uint8_t ci = tile_get_pixel(tile, tx, ty);
                if (ci == 0) continue;
                RGB5 rgb = pal->colors[ci];
                uint8_t r, g, b;
                rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect pr = {x + tx * scale, y + ty * scale, scale, scale};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &pr);
            }
        }

        /* Name below */
        ui_text_small(x, y + TILE_H * scale + 4, tile->name);

        /* Selected highlight */
        if (app->cur_tile == i) {
            SDL_SetRenderDrawColor(app->renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(app->renderer, &bg);
        }

        /* Click to select */
        if (ui_mouse_in_rect(x - 1, y - 1, TILE_W * scale + 2, TILE_H * scale + ui_line_height() + 6)
            && ui_mouse_clicked()) {
            app->cur_tile = i;
            app->view = VIEW_TILES;
        }

        x += TILE_W * scale + gap;
        if (x + TILE_W * scale > c.x + c.w) {
            x = c.x + 10;
            y += TILE_H * scale + ui_line_height() + gap;
        }
    }

    ui_panel_end();
}
