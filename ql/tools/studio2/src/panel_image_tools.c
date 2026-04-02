/*
 * panel_image_tools.c — Image tools: palette, zoom, name, export
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static bool img_name_focus = false;
static char img_name_buf[64] = {};

void draw_image_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int y = c.y;

    /* Palette (reuse) */
    draw_palette(app, c.x - 4, y, c.w + 8, &y);

    /* Properties */
    if (app->current_image < app->image_count) {
        QLImage *img = &app->images[app->current_image];

        y = ui_section(c.x - 4, y, c.w + 8, "Properties");

        /* Name */
        int lw = ui_text_width("Name:");
        int field_x = c.x + lw + 8;
        int field_w = c.w - lw - 8;
        ui_text_color(c.x, y + 3, "Name:", ui_theme.text_dim);
        if (!img_name_focus) {
            strncpy(img_name_buf, img->name, sizeof(img_name_buf) - 1);
        }
        if (ui_input_text(field_x, y, field_w, img_name_buf, sizeof(img_name_buf), &img_name_focus)) {
            strncpy(img->name, img_name_buf, sizeof(img->name) - 1);
        }
        y += ui_line_height() + 10;

        /* Size info */
        ui_text_color(c.x, y, "256 x 256  (32 KB)", ui_theme.text_dark);
        y += ui_line_height() + 10;
    }

    /* Zoom */
    y = ui_section(c.x - 4, y, c.w + 8, "Zoom");

    int zbw = 40;
    int zgap = 4;
    if (ui_button(c.x, y, zbw, ui_line_height() + 4, "1x")) app->image_zoom = 1;
    if (ui_button(c.x + zbw + zgap, y, zbw, ui_line_height() + 4, "2x")) app->image_zoom = 2;
    if (ui_button(c.x + (zbw + zgap) * 2, y, zbw, ui_line_height() + 4, "4x")) app->image_zoom = 4;

    /* Highlight active zoom */
    int active_idx = app->image_zoom == 1 ? 0 : app->image_zoom == 2 ? 1 : 2;
    int hx = c.x + active_idx * (zbw + zgap);
    SDL_Rect hl = {hx, y + ui_line_height() + 4, zbw, 2};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.tab_active.r, ui_theme.tab_active.g, ui_theme.tab_active.b, 255);
    SDL_RenderFillRect(app->renderer, &hl);

    ui_panel_end();
}
