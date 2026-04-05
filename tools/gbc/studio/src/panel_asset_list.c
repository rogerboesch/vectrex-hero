/*
 * panel_asset_list.c — Combined tile + sprite list (left panel)
 *
 * Selecting a tile activates tile layer, selecting a sprite activates sprite layer.
 * +/Del/Copy buttons for adding, removing, and duplicating assets.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_asset_list(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Assets", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    Tileset *ts = &app->tmap.tileset;
    GBCPalette *pal = &app->tmap.bg_pals[0];

    /* ── Tiles section ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Tiles");

    /* Tile management buttons */
    {
        int bx = c.x;
        int bw = 24, bh = 20, gap = 2;
        if (ui_button(bx, y, bw, bh, "+")) {
            if (ts->used_count < TSET_COUNT) {
                memset(&ts->entries[ts->used_count], 0, sizeof(TilesetEntry));
                app->cur_tset_tile = ts->used_count;
                ts->used_count++;
                app->sel_type = SEL_TILE;
                app->modified = true;
            }
        }
        bx += bw + gap;
        if (ui_button(bx, y, bw, bh, "D")) {
            if (ts->used_count > 1 && app->cur_tset_tile < ts->used_count) {
                /* Copy selected tile as new entry */
                if (ts->used_count < TSET_COUNT) {
                    ts->entries[ts->used_count] = ts->entries[app->cur_tset_tile];
                    app->cur_tset_tile = ts->used_count;
                    ts->used_count++;
                    app->sel_type = SEL_TILE;
                    app->modified = true;
                }
            }
        }
        bx += bw + gap;
        if (ui_button(bx, y, bw, bh, "X")) {
            if (ts->used_count > 1 && app->cur_tset_tile < ts->used_count) {
                for (int j = app->cur_tset_tile; j < ts->used_count - 1; j++)
                    ts->entries[j] = ts->entries[j + 1];
                ts->used_count--;
                if (app->cur_tset_tile >= ts->used_count)
                    app->cur_tset_tile = ts->used_count - 1;
                app->modified = true;
            }
        }
        y += bh + 4;
    }

    int scale = 2;
    int tile_px = 8 * scale;
    int cols = c.w / (tile_px + 2);
    if (cols < 1) cols = 1;

    for (int i = 0; i < ts->used_count; i++) {
        int col = i % cols, row = i / cols;
        int tx = c.x + col * (tile_px + 2);
        int ty = y + row * (tile_px + 2);
        if (ty > c.y + c.h / 2) break;  /* use top half for tiles */

        for (int py2 = 0; py2 < 8; py2++) for (int px2 = 0; px2 < 8; px2++) {
            uint8_t ci = tset_get_pixel(&ts->entries[i], px2, py2);
            RGB5 rgb = pal->colors[ci];
            uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {tx + px2 * scale, ty + py2 * scale, scale, scale};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
        }

        /* Selection highlight */
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

    /* ── Sprites section ── */
    int sprite_y = c.y + c.h / 2;
    y = sprite_y;
    y = ui_section(c.x - 4, y, c.w + 8, "Sprites");

    /* Sprite management buttons */
    {
        int bx = c.x;
        int bw = 24, bh = 20, gap = 2;
        if (ui_button(bx, y, bw, bh, "+")) {
            if (app->sprite_count < MAX_TILES) {
                GBCTile *t = &app->sprites[app->sprite_count];
                memset(t, 0, sizeof(GBCTile));
                snprintf(t->name, sizeof(t->name), "spr_%d", app->sprite_count);
                app->cur_sprite = app->sprite_count;
                app->sprite_count++;
                app->sel_type = SEL_SPRITE;
                app->modified = true;
            }
        }
        bx += bw + gap;
        if (ui_button(bx, y, bw, bh, "D")) {
            if (app->sprite_count > 0 && app->cur_sprite < app->sprite_count) {
                if (app->sprite_count < MAX_TILES) {
                    app->sprites[app->sprite_count] = app->sprites[app->cur_sprite];
                    GBCTile *dup = &app->sprites[app->sprite_count];
                    snprintf(dup->name, sizeof(dup->name), "%s_cp", app->sprites[app->cur_sprite].name);
                    app->cur_sprite = app->sprite_count;
                    app->sprite_count++;
                    app->sel_type = SEL_SPRITE;
                    app->modified = true;
                }
            }
        }
        bx += bw + gap;
        if (ui_button(bx, y, bw, bh, "X")) {
            if (app->sprite_count > 1 && app->cur_sprite < app->sprite_count) {
                for (int j = app->cur_sprite; j < app->sprite_count - 1; j++)
                    app->sprites[j] = app->sprites[j + 1];
                app->sprite_count--;
                if (app->cur_sprite >= app->sprite_count)
                    app->cur_sprite = app->sprite_count - 1;
                app->modified = true;
            }
        }
        y += bh + 4;
    }

    int sscale = 2;
    int spr_w = 8 * sscale, spr_h = 16 * sscale;

    for (int i = 0; i < app->sprite_count; i++) {
        GBCTile *spr = &app->sprites[i];
        GBCPalette *sp = &app->tmap.spr_pals[spr->palette < MAX_SPR_PALS ? spr->palette : 0];

        int sx = c.x, sy = y;
        /* Draw mini sprite */
        for (int py2 = 0; py2 < 16; py2++) for (int px2 = 0; px2 < 8; px2++) {
            uint8_t ci = tile_get_pixel(spr, px2, py2);
            if (ci == 0) continue;
            RGB5 rgb = sp->colors[ci];
            uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {sx + px2 * sscale, sy + py2 * sscale, sscale, sscale};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
        }

        /* Name */
        bool selected = (app->sel_type == SEL_SPRITE && app->cur_sprite == i);
        SDL_Color nc = selected ? (SDL_Color){255,255,0,255} : ui_theme.text;
        ui_text_color(sx + spr_w + 6, sy + 8, spr->name, nc);

        if (ui_mouse_in_rect(sx, sy, c.w, spr_h + 2) && ui_mouse_clicked()) {
            app->cur_sprite = i;
            app->sel_type = SEL_SPRITE;
        }

        y += spr_h + 4;
        if (y > c.y + c.h - spr_h) break;
    }

    ui_panel_end();
}
