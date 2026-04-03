/*
 * panel_tile_list.c — Tile/sprite browser with add/delete
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_tile_list(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    int bw = (tb.w - 8) / 3, gap = 4;
    if (ui_button(tb.x, tb.y, bw, tb.h, "+")) {
        if (app->tile_count < MAX_TILES) {
            GBCTile *t = &app->tiles[app->tile_count];
            memset(t, 0, sizeof(*t));
            snprintf(t->name, sizeof(t->name), "tile_%d", app->tile_count);
            app->cur_tile = app->tile_count;
            app->tile_count++;
            app->modified = true;
        }
    }
    if (ui_button(tb.x + bw + gap, tb.y, bw, tb.h, "-")) {
        if (app->tile_count > 1) {
            for (int i = app->cur_tile; i < app->tile_count - 1; i++)
                app->tiles[i] = app->tiles[i + 1];
            app->tile_count--;
            if (app->cur_tile >= app->tile_count) app->cur_tile = app->tile_count - 1;
            app->modified = true;
        }
    }
    if (ui_button(tb.x + (bw + gap) * 2, tb.y, bw, tb.h, "Dup")) {
        if (app->tile_count < MAX_TILES) {
            app->tiles[app->tile_count] = app->tiles[app->cur_tile];
            snprintf(app->tiles[app->tile_count].name, 32, "%s_cp", app->tiles[app->cur_tile].name);
            app->cur_tile = app->tile_count;
            app->tile_count++;
            app->modified = true;
        }
    }

    /* Tile list with mini previews */
    for (int i = 0; i < app->tile_count; i++) {
        GBCTile *t = &app->tiles[i];
        GBCPalette *pal = &app->tmap.spr_pals[t->palette < MAX_SPR_PALS ? t->palette : 0];

        /* Draw 16x32 preview (2x scale) */
        int prev_x = c.x, prev_y = y;
        int scale = 2;
        for (int ty = 0; ty < TILE_H; ty++) {
            for (int tx = 0; tx < TILE_W; tx++) {
                uint8_t ci = tile_get_pixel(t, tx, ty);
                if (ci == 0) continue;
                RGB5 rgb = pal->colors[ci];
                uint8_t r, g, b;
                rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect pr = {prev_x + tx * scale, prev_y + ty * scale, scale, scale};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &pr);
            }
        }

        /* Name next to preview */
        bool selected = (app->cur_tile == i);
        SDL_Color tc = selected ? (SDL_Color){255,255,0,255} : ui_theme.text;
        ui_text_color(prev_x + TILE_W * scale + 6, y + 8, t->name, tc);

        /* Click to select */
        if (ui_mouse_in_rect(c.x, y, c.w, TILE_H * scale + 2) && ui_mouse_clicked())
            app->cur_tile = i;

        y += TILE_H * scale + 4;
        if (y > c.y + c.h - TILE_H * scale) break;
    }

    ui_panel_end();
}
