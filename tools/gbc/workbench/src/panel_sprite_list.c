/*
 * panel_sprite_list.c — Sprite list + preview (left panel, sprite view)
 *
 * Lists all sprites with mini previews. +/Del/Copy buttons.
 * Zoomed preview of selected sprite below the list.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_sprite_list(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Sprites", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* ── Sprites section with toolbar buttons ── */
    {
        SDL_Rect tb = ui_section_bar(c.x - 4, y, c.w + 8, "Sprites");
        int bw = tb.h, bx = tb.x + tb.w - 3 * (bw + 2);
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
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
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_ADD, (SDL_Color){255,255,255,255});
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
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
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_COPY, (SDL_Color){255,255,255,255});
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, tb.h, "")) {
            if (app->sprite_count > 1 && app->cur_sprite < app->sprite_count) {
                for (int j = app->cur_sprite; j < app->sprite_count - 1; j++)
                    app->sprites[j] = app->sprites[j + 1];
                app->sprite_count--;
                if (app->cur_sprite >= app->sprite_count)
                    app->cur_sprite = app->sprite_count - 1;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, tb.h, ICON_TRASH, (SDL_Color){255,255,255,255});
        y = tb.y + tb.h + 10;
    }

    /* ── Sprite list ── */
    int sscale = 2;
    int spr_w = 8 * sscale, spr_h = 16 * sscale;
    int list_end = c.y + c.h - 80 - 14;  /* reserve space for preview */

    for (int i = 0; i < app->sprite_count; i++) {
        GBCTile *spr = &app->sprites[i];
        GBCPalette *sp = &app->tmap.spr_pals[spr->palette < MAX_SPR_PALS ? spr->palette : 0];

        int sx = c.x, sy = y;
        if (sy + spr_h > list_end) break;

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
    }

    /* ── Zoomed preview of selected sprite (centered) ── */
    if (app->sel_type == SEL_SPRITE && app->cur_sprite < app->sprite_count) {
        GBCTile *spr = &app->sprites[app->cur_sprite];
        GBCPalette *sp = &app->tmap.spr_pals[spr->palette < MAX_SPR_PALS ? spr->palette : 0];

        int prev_scale = 4;
        int prev_w = 8 * prev_scale;
        int prev_h = 16 * prev_scale;
        int prev_x = c.x + (c.w - prev_w) / 2;
        int prev_y = c.y + c.h - prev_h - 20;

        /* Black background */
        SDL_Rect bg = {prev_x, prev_y, prev_w, prev_h};
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(app->renderer, &bg);

        for (int py2 = 0; py2 < 16; py2++) {
            for (int px2 = 0; px2 < 8; px2++) {
                uint8_t ci = tile_get_pixel(spr, px2, py2);
                RGB5 rgb = sp->colors[ci];
                uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect pr = {prev_x + px2 * prev_scale, prev_y + py2 * prev_scale,
                               prev_scale, prev_scale};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &pr);
            }
        }

        /* Border */
        SDL_SetRenderDrawColor(app->renderer, ui_theme.border.r, ui_theme.border.g, ui_theme.border.b, 255);
        SDL_RenderDrawRect(app->renderer, &bg);

        /* Label centered below */
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "%s  pal %d", spr->name, spr->palette);
        int lbl_w = ui_text_width(lbl);
        ui_text_small(prev_x + (prev_w - lbl_w) / 2, prev_y + prev_h + 4, lbl);
    }

    ui_panel_end();
}
