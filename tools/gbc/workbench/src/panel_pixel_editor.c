/*
 * panel_pixel_editor.c — Pixel editor for tiles (8x8) and sprites (8x16)
 *
 * Edits whichever asset is currently selected in the asset list.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

/* Flood fill for 8x8 tileset entry */
static void flood_fill_tset(TilesetEntry *te, int sx, int sy,
                             uint8_t new_color) {
    uint8_t old_color = tset_get_pixel(te, sx, sy);
    if (old_color == new_color) return;
    int16_t stk[128][2];
    int sp = 0;
    stk[sp][0] = sx; stk[sp][1] = sy; sp++;
    while (sp > 0) {
        sp--;
        int x = stk[sp][0], y = stk[sp][1];
        if (x < 0 || x >= 8 || y < 0 || y >= 8) continue;
        if (tset_get_pixel(te, x, y) != old_color) continue;
        tset_set_pixel(te, x, y, new_color);
        if (sp + 4 < 128) {
            stk[sp][0]=x+1; stk[sp][1]=y; sp++;
            stk[sp][0]=x-1; stk[sp][1]=y; sp++;
            stk[sp][0]=x; stk[sp][1]=y+1; sp++;
            stk[sp][0]=x; stk[sp][1]=y-1; sp++;
        }
    }
}

/* Flood fill for 8x16 sprite */
static void flood_fill_sprite(GBCTile *spr, int sx, int sy,
                               uint8_t new_color) {
    uint8_t old_color = tile_get_pixel(spr, sx, sy);
    if (old_color == new_color) return;
    int16_t stk[256][2];
    int sp = 0;
    stk[sp][0] = sx; stk[sp][1] = sy; sp++;
    while (sp > 0) {
        sp--;
        int x = stk[sp][0], y = stk[sp][1];
        if (x < 0 || x >= TILE_W || y < 0 || y >= TILE_H) continue;
        if (tile_get_pixel(spr, x, y) != old_color) continue;
        tile_set_pixel(spr, x, y, new_color);
        if (sp + 4 < 256) {
            stk[sp][0]=x+1; stk[sp][1]=y; sp++;
            stk[sp][0]=x-1; stk[sp][1]=y; sp++;
            stk[sp][0]=x; stk[sp][1]=y+1; sp++;
            stk[sp][0]=x; stk[sp][1]=y-1; sp++;
        }
    }
}

void draw_pixel_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int bh = tb.h, ibw = bh;

    if (app->sel_type == SEL_TILE) {
        /* Editing a tileset entry (8x8) */
        if (app->cur_tset_tile >= app->tmap.tileset.used_count) { ui_panel_end(); return; }
        TilesetEntry *te = &app->tmap.tileset.entries[app->cur_tset_tile];
        GBCPalette *pal = &app->tmap.bg_pals[te->palette < MAX_BG_PALS ? te->palette : 0];

        char info[32]; snprintf(info, sizeof(info), "Tile %d (8x8)", app->cur_tset_tile);
        ui_text_color(tb.x + 4, tb.y + 2, info, ui_theme.text);

        /* Clear button */
        int bx = tb.x + tb.w - (ibw + 2);
        if (ui_button(bx, tb.y, ibw, bh, "")) { memset(te->data, 0, 16); app->modified = true; }
        ui_icon_centered(bx, tb.y, ibw, bh, ICON_ERASER, ui_theme.text);

        int tile_w = 8, tile_h = 8;
        int cell = c.h / tile_h;
        if (cell > c.w / tile_w) cell = c.w / tile_w;
        if (cell < 4) cell = 4; if (cell > 40) cell = 40;
        int tw = tile_w * cell, th = tile_h * cell;
        int ox = c.x + (c.w - tw) / 2, oy = c.y + (c.h - th) / 2;

        /* Draw pixels */
        for (int ty = 0; ty < tile_h; ty++) for (int tx = 0; tx < tile_w; tx++) {
            uint8_t ci = tset_get_pixel(te, tx, ty);
            RGB5 rgb = pal->colors[ci]; uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {ox + tx * cell, oy + ty * cell, cell, cell};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
            SDL_SetRenderDrawColor(app->renderer, 40, 40, 40, 255);
            SDL_RenderDrawRect(app->renderer, &pr);
        }

        /* Mouse paint */
        if (ui_mouse_in_rect(ox, oy, tw, th)) {
            int mx, my; ui_mouse_pos(&mx, &my);
            int tx = (mx - ox) / cell, ty = (my - oy) / cell;
            if (tx >= 0 && tx < tile_w && ty >= 0 && ty < tile_h) {
                bool ctrl = (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) != 0;
                if (ui_mouse_clicked() && ctrl) {
                    flood_fill_tset(te, tx, ty, app->cur_color);
                    app->modified = true;
                }
                else if (ui_mouse_down() && !ctrl) {
                    tset_set_pixel(te, tx, ty, app->cur_color);
                    app->modified = true;
                }
                if (ui_mouse_right_clicked()) app->cur_color = tset_get_pixel(te, tx, ty);
                char tip[16]; snprintf(tip, sizeof(tip), "(%d,%d) c:%d", tx, ty, tset_get_pixel(te, tx, ty));
                ui_tooltip(tip);
            }
        }
    } else {
        /* Editing a sprite (8x16) */
        if (app->cur_sprite >= app->sprite_count) { ui_panel_end(); return; }
        GBCTile *spr = &app->sprites[app->cur_sprite];
        GBCPalette *pal = &app->tmap.spr_pals[spr->palette < MAX_SPR_PALS ? spr->palette : 0];

        char info[32]; snprintf(info, sizeof(info), "%s (8x16)", spr->name);
        ui_text_color(tb.x + 4, tb.y + 2, info, ui_theme.text);

        /* Buttons: flip-h, flip-v, clear — right-aligned */
        int bx = tb.x + tb.w - 3 * (ibw + 2);
        if (ui_button(bx, tb.y, ibw, bh, "H")) { tile_flip_h(spr); app->modified = true; }
        bx += ibw + 2;
        if (ui_button(bx, tb.y, ibw, bh, "V")) { tile_flip_v(spr); app->modified = true; }
        bx += ibw + 2;
        if (ui_button(bx, tb.y, ibw, bh, "")) { tile_clear(spr); app->modified = true; }
        ui_icon_centered(bx, tb.y, ibw, bh, ICON_ERASER, ui_theme.text);

        int tile_w = 8, tile_h = 16;
        int cell = c.h / tile_h;
        if (cell > c.w / tile_w) cell = c.w / tile_w;
        if (cell < 4) cell = 4; if (cell > 32) cell = 32;
        int tw = tile_w * cell, th = tile_h * cell;
        int ox = c.x + (c.w - tw) / 2, oy = c.y + (c.h - th) / 2;

        /* Draw pixels */
        for (int ty = 0; ty < tile_h; ty++) for (int tx = 0; tx < tile_w; tx++) {
            uint8_t ci = tile_get_pixel(spr, tx, ty);
            RGB5 rgb = pal->colors[ci]; uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {ox + tx * cell, oy + ty * cell, cell, cell};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
            SDL_SetRenderDrawColor(app->renderer, 40, 40, 40, 255);
            SDL_RenderDrawRect(app->renderer, &pr);
        }

        /* 8x8 boundary */
        SDL_SetRenderDrawColor(app->renderer, 80, 80, 120, 255);
        SDL_RenderDrawLine(app->renderer, ox, oy + 8 * cell, ox + tw, oy + 8 * cell);

        /* Mouse paint */
        if (ui_mouse_in_rect(ox, oy, tw, th)) {
            int mx, my; ui_mouse_pos(&mx, &my);
            int tx = (mx - ox) / cell, ty = (my - oy) / cell;
            if (tx >= 0 && tx < tile_w && ty >= 0 && ty < tile_h) {
                bool ctrl = (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) != 0;
                if (ui_mouse_clicked() && ctrl) {
                    flood_fill_sprite(spr, tx, ty, app->cur_color);
                    app->modified = true;
                }
                else if (ui_mouse_down() && !ctrl) {
                    tile_set_pixel(spr, tx, ty, app->cur_color);
                    app->modified = true;
                }
                if (ui_mouse_right_clicked()) app->cur_color = tile_get_pixel(spr, tx, ty);
                char tip[16]; snprintf(tip, sizeof(tip), "(%d,%d) c:%d", tx, ty, tile_get_pixel(spr, tx, ty));
                ui_tooltip(tip);
            }
        }
    }

    ui_panel_end();
}
