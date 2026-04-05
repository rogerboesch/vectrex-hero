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
        RGB5 rgb = pal->colors[i];
        uint8_t r, g, b; rgb5_to_rgb8(rgb, &r, &g, &b);
        SDL_Color sc = {r, g, b, 255};
        int sw = 28;
        if (ui_color_swatch(c.x, y, sw, sc, app->cur_color == i))
            app->cur_color = i;
        char lbl[8]; snprintf(lbl, sizeof(lbl), "%d", i);
        ui_text(c.x + sw + 6, y + 4, lbl);
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

    y += 4;

    /* Properties */
    if (app->sel_type == SEL_TILE) {
        y = ui_section(c.x - 4, y, c.w + 8, "Tile Properties");
        char info[32];
        snprintf(info, sizeof(info), "Tile %d  (8x8, 16 bytes)", app->cur_tset_tile);
        ui_text_color(c.x, y, info, ui_theme.text_dim);
        y += ui_line_height() + 8;

        TilesetEntry *te = &app->tmap.tileset.entries[app->cur_tset_tile];
        int p = (int)te->palette;
        if (ui_input_int_ex(c.x, y, c.w, "Pal:", ui_text_width("Pal:"), &p, 1, 0, MAX_BG_PALS - 1)) {
            te->palette = (uint8_t)p;
            app->modified = true;
        }
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

            int p = spr->palette;
            if (ui_input_int_ex(c.x, y, c.w, "Pal:", ui_text_width("Pal:"), &p, 1, 0, MAX_SPR_PALS - 1)) {
                spr->palette = p;
                app->modified = true;
            }
            y += ui_line_height() + 8;

            char info[32];
            snprintf(info, sizeof(info), "8x16, %d bytes", TILE_BYTES);
            ui_text_color(c.x, y, info, ui_theme.text_dark);
        }
    }

    ui_panel_end();
}
