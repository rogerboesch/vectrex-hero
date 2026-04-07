/*
 * panel_asset_list.c — Tile list + BG palette selector (left panel, level view)
 *
 * Selecting a tile activates tile layer.
 * +/Del/Copy buttons for adding, removing, and duplicating tiles.
 * BG Palette selector below the tile grid.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_asset_list(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Assets", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    Tileset *ts = &app->tmap.tileset;

    /* ── Tiles section with toolbar buttons ── */
    {
        SDL_Rect tb = ui_section_bar(c.x - 4, y, c.w + 8, "Tiles");
        int bw = tb.h, bx = tb.x + tb.w - 3 * (bw + 2);
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
            if (ts->used_count < TSET_COUNT) {
                memset(&ts->entries[ts->used_count], 0, sizeof(TilesetEntry));
                app->cur_tset_tile = ts->used_count;
                ts->used_count++;
                app->sel_type = SEL_TILE;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_ADD, (SDL_Color){255,255,255,255});
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
            if (ts->used_count > 1 && app->cur_tset_tile < ts->used_count) {
                if (ts->used_count < TSET_COUNT) {
                    ts->entries[ts->used_count] = ts->entries[app->cur_tset_tile];
                    app->cur_tset_tile = ts->used_count;
                    ts->used_count++;
                    app->sel_type = SEL_TILE;
                    app->modified = true;
                }
            }
        }
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_COPY, (SDL_Color){255,255,255,255});
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
            if (ts->used_count > 1 && app->cur_tset_tile < ts->used_count) {
                for (int j = app->cur_tset_tile; j < ts->used_count - 1; j++)
                    ts->entries[j] = ts->entries[j + 1];
                ts->used_count--;
                if (app->cur_tset_tile >= ts->used_count)
                    app->cur_tset_tile = ts->used_count - 1;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_TRASH, (SDL_Color){255,255,255,255});
        y = tb.y + tb.h + 10;
    }

    int scale = 2;
    int tile_px = 8 * scale;
    int cols = c.w / (tile_px + 2);
    if (cols < 1) cols = 1;

    /* Calculate how much space tiles + preview + palette need */
    int tile_area_end = c.y + c.h - 64 - 14 - 120;  /* reserve for preview + palette */

    for (int i = 0; i < ts->used_count; i++) {
        int col = i % cols, row = i / cols;
        int tx = c.x + col * (tile_px + 2);
        int ty = y + row * (tile_px + 2);
        if (ty + tile_px > tile_area_end) break;

        /* Render tile with its assigned palette */
        uint8_t pal_idx = ts->entries[i].palette < MAX_BG_PALS ? ts->entries[i].palette : 0;
        GBCPalette *pal = &app->tmap.bg_pals[pal_idx];
        for (int py2 = 0; py2 < 8; py2++) for (int px2 = 0; px2 < 8; px2++) {
            uint8_t ci = tset_get_pixel(&ts->entries[i], px2, py2);
            RGB5 rgb = pal->colors[ci];
            uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {tx + px2 * scale, ty + py2 * scale, scale, scale};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
        }

        bool selected = (app->sel_type == SEL_TILE && app->cur_tset_tile == i);
        if (selected) {
            SDL_SetRenderDrawColor(app->renderer, 255, 255, 0, 255);
            SDL_Rect sr = {tx - 1, ty - 1, tile_px + 2, tile_px + 2};
            SDL_RenderDrawRect(app->renderer, &sr);
        }

        if (ui_mouse_in_rect(tx, ty, tile_px, tile_px) && ui_mouse_clicked()) {
            app->cur_tset_tile = i;
            app->sel_type = SEL_TILE;
        }
    }

    /* ── Zoomed preview of selected tile (64x64, centered) ── */
    {
        int prev_size = 64;
        int prev_scale = prev_size / 8;
        int prev_x = c.x + (c.w - prev_size) / 2;
        int prev_y = c.y + c.h - prev_size - 120 - 24;

        if (app->sel_type == SEL_TILE && app->cur_tset_tile < ts->used_count) {
            TilesetEntry *te = &ts->entries[app->cur_tset_tile];
            uint8_t pal_idx = te->palette < MAX_BG_PALS ? te->palette : 0;
            GBCPalette *tp = &app->tmap.bg_pals[pal_idx];

            SDL_Rect bg = {prev_x, prev_y, prev_size, prev_size};
            SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(app->renderer, &bg);

            for (int py2 = 0; py2 < 8; py2++) {
                for (int px2 = 0; px2 < 8; px2++) {
                    uint8_t ci = tset_get_pixel(te, px2, py2);
                    RGB5 rgb = tp->colors[ci];
                    uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
                    SDL_Rect pr = {prev_x + px2 * prev_scale, prev_y + py2 * prev_scale,
                                   prev_scale, prev_scale};
                    SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                    SDL_RenderFillRect(app->renderer, &pr);
                }
            }

            SDL_SetRenderDrawColor(app->renderer, ui_theme.border.r, ui_theme.border.g, ui_theme.border.b, 255);
            SDL_RenderDrawRect(app->renderer, &bg);

            /* Label centered below preview */
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "#%d  pal %d", app->cur_tset_tile, pal_idx);
            int lbl_w = ui_text_width(lbl);
            ui_text_small(prev_x + (prev_size - lbl_w) / 2, prev_y + prev_size + 4, lbl);
        }
    }

    /* ── BG Palette selector (only in level view) ── */
    if (app->view == VIEW_LEVELS) {
        int pal_y = c.y + c.h - 110;
        pal_y = ui_section(c.x - 4, pal_y, c.w + 8, "BG Palette");

        int sw = 28, gap = 4;
        int pcols = (c.w + gap) / (sw + gap);
        if (pcols < 1) pcols = 1;
        for (int i = 0; i < MAX_BG_PALS; i++) {
            int col = i % pcols, row = i / pcols;
            int sx = c.x + col * (sw + gap);
            int sy = pal_y + row * (sw + gap);
            int stripe_h = sw / 4;
            for (int ci = 0; ci < 4; ci++) {
                RGB5 rgb = app->tmap.bg_pals[i].colors[ci];
                uint8_t r, g, b;
                rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect sr = {sx, sy + ci * stripe_h, sw, stripe_h};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &sr);
            }
            bool selected = (app->cur_palette == i);
            SDL_Color border = selected ? ui_theme.swatch_sel : ui_theme.border;
            SDL_Rect br = {sx, sy, sw, sw};
            SDL_SetRenderDrawColor(app->renderer, border.r, border.g, border.b, 255);
            SDL_RenderDrawRect(app->renderer, &br);
            if (selected) {
                SDL_Rect br2 = {sx + 1, sy + 1, sw - 2, sw - 2};
                SDL_RenderDrawRect(app->renderer, &br2);
            }
            if (ui_mouse_in_rect(sx, sy, sw, sw) && ui_mouse_clicked())
                app->cur_palette = i;
        }
        int pal_rows = (MAX_BG_PALS + pcols - 1) / pcols;
        pal_y += pal_rows * (sw + gap) + 2;

        /* Palette label + Apply to all */
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "Pal: %d", app->cur_palette);
        int lw = ui_text_color(c.x, pal_y, lbl, ui_theme.text_dim);

        if (ui_button_auto(c.x + lw + 10, pal_y - 2, "Apply to all")) {
            TilemapLevel *lvl = (app->cur_level >= 0 && app->cur_level < app->tmap.level_count)
                                ? &app->tmap.levels[app->cur_level] : NULL;
            if (lvl) {
                for (int ay = 0; ay < lvl->height; ay++)
                    for (int ax = 0; ax < lvl->width; ax++)
                        lvl->palettes[ay][ax] = (uint8_t)app->cur_palette;
                app->modified = true;
            }
        }
    }

    ui_panel_end();
}
