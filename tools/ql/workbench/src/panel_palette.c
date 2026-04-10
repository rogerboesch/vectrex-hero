/*
 * panel_palette.c — 8-color QL palette with swatches
 *
 * Shared between sprite and image editors.
 * Writes to app->current_color.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_palette(App *app, int x, int y, int w, int *out_y) {
    y = ui_section(x, y, w, "Palette");

    int cx = x + 4;
    for (int i = 0; i < 8; i++) {
        int sw = 22;
        if (ui_color_swatch(cx, y, sw, QL_SDL_COLORS[i], app->current_color == i)) {
            app->current_color = i;
        }
        char label[32];
        snprintf(label, sizeof(label), "%d: %s", i, QL_COLOR_NAMES[i]);
        ui_text(cx + sw + 6, y + 2, label);
        y += sw + 3;
    }

    /* Number key shortcuts */
    for (int i = 0; i < 8; i++) {
        if (ui_key_pressed(SDLK_0 + i))
            app->current_color = i;
    }

    y += 8;
    *out_y = y;
}
