/*
 * panel_palette.c — CGB palette editor (RGB555)
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_palette_editor(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Palettes", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int sw = 36, gap = 6;

    /* Sprite palettes */
    y = ui_section(c.x - 4, y, c.w + 8, "Sprite Palettes");
    for (int pi = 0; pi < MAX_SPR_PALS; pi++) {
        char label[16];
        snprintf(label, sizeof(label), "SP%d:", pi);
        ui_text_color(c.x, y + 8, label, ui_theme.text_dim);
        for (int ci = 0; ci < COLORS_PER_PAL; ci++) {
            RGB5 rgb = app->tmap.spr_pals[pi].colors[ci];
            uint8_t r, g, b;
            rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Color sc = {r, g, b, 255};
            int sx = c.x + 40 + ci * (sw + gap);
            bool sel = (app->cur_pal_type == 1 && app->cur_pal_idx == pi);
            if (ui_color_swatch(sx, y, sw, sc, sel && app->cur_color == ci)) {
                app->cur_pal_type = 1;
                app->cur_pal_idx = pi;
                app->cur_color = ci;
            }
            /* RGB values below */
            char rv[16];
            snprintf(rv, sizeof(rv), "%d,%d,%d", rgb.r, rgb.g, rgb.b);
            ui_text_small(sx, y + sw + 2, rv);
        }
        y += sw + ui_line_height() + 8;
    }

    y += 8;

    /* BG palettes */
    y = ui_section(c.x - 4, y, c.w + 8, "Background Palettes");
    for (int pi = 0; pi < MAX_BG_PALS; pi++) {
        char label[16];
        snprintf(label, sizeof(label), "BG%d:", pi);
        ui_text_color(c.x, y + 8, label, ui_theme.text_dim);
        for (int ci = 0; ci < COLORS_PER_PAL; ci++) {
            RGB5 rgb = app->tmap.bg_pals[pi].colors[ci];
            uint8_t r, g, b;
            rgb5_to_rgb8(rgb, &r, &g, &b);
            SDL_Color sc = {r, g, b, 255};
            int sx = c.x + 40 + ci * (sw + gap);
            bool sel = (app->cur_pal_type == 0 && app->cur_pal_idx == pi);
            if (ui_color_swatch(sx, y, sw, sc, sel && app->cur_color == ci)) {
                app->cur_pal_type = 0;
                app->cur_pal_idx = pi;
                app->cur_color = ci;
            }
            char rv[16];
            snprintf(rv, sizeof(rv), "%d,%d,%d", rgb.r, rgb.g, rgb.b);
            ui_text_small(sx, y + sw + 2, rv);
        }
        y += sw + ui_line_height() + 8;
    }

    ui_panel_end();
}
