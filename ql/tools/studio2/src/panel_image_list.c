/*
 * panel_image_list.c — Image list with add/delete/duplicate
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_image_list(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* +/-/Dup in the title bar */
    int bbw = (tb.w - 8) / 3;
    if (ui_button(tb.x, tb.y, bbw, tb.h, "+")) {
        if (app->image_count < MAX_IMAGES) {
            char name[64];
            snprintf(name, sizeof(name), "image_%d", app->image_count);
            image_init(&app->images[app->image_count], name);
            app->current_image = app->image_count;
            app->image_count++;
            app->image_tex_dirty = true;
        }
    }
    if (ui_button(tb.x + bbw + 4, tb.y, bbw, tb.h, "-")) {
        if (app->image_count > 0) {
            for (int i = app->current_image; i < app->image_count - 1; i++)
                app->images[i] = app->images[i + 1];
            app->image_count--;
            if (app->current_image >= app->image_count)
                app->current_image = app->image_count > 0 ? app->image_count - 1 : 0;
            app->image_tex_dirty = true;
        }
    }
    if (ui_button(tb.x + (bbw + 4) * 2, tb.y, bbw, tb.h, "Dup")) {
        if (app->image_count < MAX_IMAGES && app->image_count > 0) {
            app->images[app->image_count] = app->images[app->current_image];
            /* Generate next available image_N name */
            char name[64];
            int n = app->image_count;
            for (;;) {
                snprintf(name, sizeof(name), "image_%d", n);
                int exists = 0;
                for (int i = 0; i < app->image_count; i++) {
                    if (strcmp(app->images[i].name, name) == 0) { exists = 1; break; }
                }
                if (!exists) break;
                n++;
            }
            strncpy(app->images[app->image_count].name, name, 63);
            app->current_image = app->image_count;
            app->image_count++;
            app->image_tex_dirty = true;
        }
    }

    /* Image list */
    for (int i = 0; i < app->image_count; i++) {
        if (ui_selectable(c.x, y, c.w, app->images[i].name, app->current_image == i)) {
            app->current_image = i;
            app->image_tex_dirty = true;
        }
        y += ui_line_height() + 2;
        if (y > c.y + c.h - ui_line_height()) break;
    }

    ui_panel_end();
}
