/*
 * panel_tile_tools.c — Color picker, name, palette selector, preview
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_tile_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    if (app->cur_tile >= app->tile_count) { ui_panel_end(); return; }
    GBCTile *tile = &app->tiles[app->cur_tile];
    GBCPalette *pal = &app->spr_pals[tile->palette < MAX_SPR_PALS ? tile->palette : 0];

    /* Color picker */
    y = ui_section(c.x - 4, y, c.w + 8, "Color");
    for (int i = 0; i < COLORS_PER_PAL; i++) {
        RGB5 rgb = pal->colors[i];
        uint8_t r, g, b;
        rgb5_to_rgb8(rgb, &r, &g, &b);
        SDL_Color sc = {r, g, b, 255};
        int sw = 28;
        if (ui_color_swatch(c.x, y, sw, sc, app->cur_color == i))
            app->cur_color = i;
        char label[16];
        snprintf(label, sizeof(label), "%d", i);
        ui_text(c.x + sw + 6, y + 4, label);
        y += sw + 4;
    }

    /* Number key shortcuts */
    for (int i = 0; i < 4; i++) {
        if (ui_key_pressed(SDLK_0 + i)) app->cur_color = i;
    }

    y += 8;

    /* Properties */
    y = ui_section(c.x - 4, y, c.w + 8, "Properties");

    /* Name */
    ui_text_color(c.x, y + 3, "Name:", ui_theme.text_dim);
    if (!app->name_focus) strncpy(app->name_buf, tile->name, sizeof(app->name_buf) - 1);
    if (ui_input_text(c.x + 44, y, c.w - 44, app->name_buf, sizeof(app->name_buf), &app->name_focus))
        strncpy(tile->name, app->name_buf, sizeof(tile->name) - 1);
    y += ui_line_height() + 8;

    /* Palette selector */
    if (ui_input_int_ex(c.x, y, c.w, "Pal:", ui_text_width("Pal:"),
                         &tile->palette, 1, 0, MAX_SPR_PALS - 1))
        app->modified = true;
    y += ui_line_height() + 12;

    /* Preview at 4x */
    y = ui_section(c.x - 4, y, c.w + 8, "Preview");
    int scale = 4;
    int prev_w = TILE_W * scale, prev_h = TILE_H * scale;
    SDL_Rect bg = {c.x, y, prev_w + 2, prev_h + 2};
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    for (int ty = 0; ty < TILE_H; ty++) {
        for (int tx = 0; tx < TILE_W; tx++) {
            uint8_t ci = tile_get_pixel(tile, tx, ty);
            if (ci == 0) continue;
            RGB5 rgb = pal->colors[ci];
            uint8_t r, g, b;
            rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Rect pr = {c.x + 1 + tx * scale, y + 1 + ty * scale, scale, scale};
            SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
            SDL_RenderFillRect(app->renderer, &pr);
        }
    }
    y += prev_h + 8;

    /* Size info */
    char info[32];
    snprintf(info, sizeof(info), "%dx%d  %d bytes", TILE_W, TILE_H, TILE_BYTES);
    ui_text_color(c.x, y, info, ui_theme.text_dark);

    ui_panel_end();
}
