/*
 * panel_editor_tools.c — Color picker + properties for tile/sprite editor
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_editor_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    GBCPalette *pal;
    if (app->sel_type == SEL_TILE) {
        TilesetEntry *te = &app->tmap.tileset.entries[app->cur_tset_tile];
        pal = &app->tmap.bg_pals[te->palette < MAX_BG_PALS ? te->palette : 0];
    } else {
        GBCTile *spr = &app->sprites[app->cur_sprite < app->sprite_count ? app->cur_sprite : 0];
        pal = &app->tmap.spr_pals[spr->palette < MAX_SPR_PALS ? spr->palette : 0];
    }

    /* Color picker */
    y = ui_section(c.x - 4, y, c.w + 8, "Color");
    for (int i = 0; i < COLORS_PER_PAL; i++) {
        RGB5 rgb5 = pal->colors[i];
        uint8_t r8, g8, b8; rgb5_to_rgb8(rgb5, &r8, &g8, &b8);
        SDL_Color sc = {r8, g8, b8, 255};
        int sw = 28;
        if (ui_color_swatch(c.x, y, sw, sc, app->cur_color == i))
            app->cur_color = i;

        /* Color index + name + hex + rgb */
        char lbl[64];
        const char *name = "";
        if (r8 == 0 && g8 == 0 && b8 == 0) name = "Black";
        else if (r8 >= 240 && g8 >= 240 && b8 >= 240) name = "White";
        else if (r8 > g8 + b8) name = (r8 > 160) ? "Red" : "DkRed";
        else if (g8 > r8 + b8) name = (g8 > 160) ? "Green" : "DkGreen";
        else if (b8 > r8 + g8) name = (b8 > 160) ? "Blue" : "DkBlue";
        else if (r8 > 100 && g8 > 100 && b8 < 60) name = "Yellow";
        else if (r8 > 100 && b8 > 100 && g8 < 60) name = "Magenta";
        else if (g8 > 100 && b8 > 100 && r8 < 60) name = "Cyan";
        else if (r8 > 100 && g8 > 60 && b8 < 60) name = "Orange";
        else if (r8 > 60 && g8 > 60 && b8 > 60) name = "Gray";
        else name = "Custom";

        snprintf(lbl, sizeof(lbl), "%d  #%02X%02X%02X (%s)", i, r8, g8, b8, name);
        ui_text_color(c.x + sw + 6, y + 4, lbl, (app->cur_color == i) ? ui_theme.text : ui_theme.text_dim);
        y += sw + 4;
    }

    for (int i = 0; i < 4; i++)
        if (ui_key_pressed(SDLK_0 + i)) app->cur_color = i;

    /* Edit selected color's RGB5 values */
    y += 4;
    {
        RGB5 *rgb = &pal->colors[app->cur_color];
        int r = rgb->r, g = rgb->g, b = rgb->b;
        int lw = ui_text_width("R:");
        if (ui_input_int_ex(c.x, y, c.w, "R:", lw, &r, 1, 0, 31)) { rgb->r = (uint8_t)r; app->modified = true; }
        y += ui_line_height() + 2;
        if (ui_input_int_ex(c.x, y, c.w, "G:", lw, &g, 1, 0, 31)) { rgb->g = (uint8_t)g; app->modified = true; }
        y += ui_line_height() + 2;
        if (ui_input_int_ex(c.x, y, c.w, "B:", lw, &b, 1, 0, 31)) { rgb->b = (uint8_t)b; app->modified = true; }
        y += ui_line_height() + 2;
    }

    y += 12;

    /* Properties */
    if (app->sel_type == SEL_TILE) {
        y = ui_section(c.x - 4, y, c.w + 8, "Tile Properties");
        char info[32];
        snprintf(info, sizeof(info), "Tile %d  (8x8, 16 bytes)", app->cur_tset_tile);
        ui_text_color(c.x, y, info, ui_theme.text_dim);
        y += ui_line_height() + 8;

        TilesetEntry *te = &app->tmap.tileset.entries[app->cur_tset_tile];
        int cur_p = (int)te->palette;

        /* Palette swatches */
        int sw = 28, gap = 4;
        int cols = (c.w + gap) / (sw + gap);
        if (cols < 1) cols = 1;
        for (int i = 0; i < MAX_BG_PALS; i++) {
            int col = i % cols, row = i / cols;
            int sx = c.x + col * (sw + gap);
            int sy = y + row * (sw + gap);
            int stripe_h = sw / 4;
            for (int ci = 0; ci < 4; ci++) {
                RGB5 rgb = app->tmap.bg_pals[i].colors[ci];
                uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
                SDL_Rect sr = {sx, sy + ci * stripe_h, sw, stripe_h};
                SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                SDL_RenderFillRect(app->renderer, &sr);
            }
            bool selected = (cur_p == i);
            SDL_Color border = selected ? ui_theme.swatch_sel : ui_theme.border;
            SDL_Rect br = {sx, sy, sw, sw};
            SDL_SetRenderDrawColor(app->renderer, border.r, border.g, border.b, 255);
            SDL_RenderDrawRect(app->renderer, &br);
            if (selected) {
                SDL_Rect br2 = {sx + 1, sy + 1, sw - 2, sw - 2};
                SDL_RenderDrawRect(app->renderer, &br2);
            }
            if (ui_mouse_in_rect(sx, sy, sw, sw) && ui_mouse_clicked()) {
                te->palette = (uint8_t)i;
                app->modified = true;
            }
        }
        int pal_rows = (MAX_BG_PALS + cols - 1) / cols;
        y += pal_rows * (sw + gap) + 4;
    } else {
        y = ui_section(c.x - 4, y, c.w + 8, "Sprite Properties");
        if (app->cur_sprite < app->sprite_count) {
            GBCTile *spr = &app->sprites[app->cur_sprite];

            static bool name_focus = false;
            static char name_buf[32] = {};
            if (!name_focus) strncpy(name_buf, spr->name, sizeof(name_buf) - 1);
            ui_text_color(c.x, y + 3, "Name:", ui_theme.text_dim);
            if (ui_input_text(c.x + 44, y, c.w - 44, name_buf, sizeof(name_buf), &name_focus))
                strncpy(spr->name, name_buf, sizeof(spr->name) - 1);
            y += ui_line_height() + 8;

            int cur_p = spr->palette;

            /* Sprite palette swatches */
            int sw = 28, gap = 4;
            int cols = (c.w + gap) / (sw + gap);
            if (cols < 1) cols = 1;
            for (int i = 0; i < MAX_SPR_PALS; i++) {
                int col = i % cols, row = i / cols;
                int sx = c.x + col * (sw + gap);
                int sy = y + row * (sw + gap);
                int stripe_h = sw / 4;
                for (int ci = 0; ci < 4; ci++) {
                    RGB5 rgb = app->tmap.spr_pals[i].colors[ci];
                    uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
                    SDL_Rect sr = {sx, sy + ci * stripe_h, sw, stripe_h};
                    SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                    SDL_RenderFillRect(app->renderer, &sr);
                }
                bool selected = (cur_p == i);
                SDL_Color border = selected ? ui_theme.swatch_sel : ui_theme.border;
                SDL_Rect br = {sx, sy, sw, sw};
                SDL_SetRenderDrawColor(app->renderer, border.r, border.g, border.b, 255);
                SDL_RenderDrawRect(app->renderer, &br);
                if (selected) {
                    SDL_Rect br2 = {sx + 1, sy + 1, sw - 2, sw - 2};
                    SDL_RenderDrawRect(app->renderer, &br2);
                }
                if (ui_mouse_in_rect(sx, sy, sw, sw) && ui_mouse_clicked()) {
                    spr->palette = i;
                    app->modified = true;
                }
            }
            int pal_rows = (MAX_SPR_PALS + cols - 1) / cols;
            y += pal_rows * (sw + gap) + 4;

            char info2[32];
            snprintf(info2, sizeof(info2), "8x16, %d bytes", TILE_BYTES);
            ui_text_color(c.x, y, info2, ui_theme.text_dark);
        }
    }

    ui_panel_end();
}
