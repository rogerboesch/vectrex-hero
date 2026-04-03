/*
 * panel_sprite_list.c — Sprite list with add/delete/duplicate
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_sprite_list(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* +/-/Dup in the title bar */
    int bbw = (tb.w - 8) / 3;
    if (ui_button(tb.x, tb.y, bbw, tb.h, "+")) {
        if (app->sprite_count < MAX_SPRITES) {
            char name[64];
            snprintf(name, sizeof(name), "sprite_%d", app->sprite_count);
            sprite_init(&app->sprites[app->sprite_count], name, 10, 20);
            app->current_sprite = app->sprite_count;
            app->sprite_count++;
        }
    }
    if (ui_button(tb.x + bbw + 4, tb.y, bbw, tb.h, "-")) {
        if (app->sprite_count > 1) {
            for (int i = app->current_sprite; i < app->sprite_count - 1; i++)
                app->sprites[i] = app->sprites[i + 1];
            app->sprite_count--;
            if (app->current_sprite >= app->sprite_count)
                app->current_sprite = app->sprite_count - 1;
        }
    }
    if (ui_button(tb.x + (bbw + 4) * 2, tb.y, bbw, tb.h, "Dup")) {
        if (app->sprite_count < MAX_SPRITES) {
            app->sprites[app->sprite_count] = app->sprites[app->current_sprite];
            /* Generate next available sprite_N name */
            char name[64];
            int n = app->sprite_count;
            for (;;) {
                snprintf(name, sizeof(name), "sprite_%d", n);
                int exists = 0;
                for (int i = 0; i < app->sprite_count; i++) {
                    if (strcmp(app->sprites[i].name, name) == 0) { exists = 1; break; }
                }
                if (!exists) break;
                n++;
            }
            strncpy(app->sprites[app->sprite_count].name, name, 63);
            app->current_sprite = app->sprite_count;
            app->sprite_count++;
        }
    }

    /* Sprite list */
    for (int i = 0; i < app->sprite_count; i++) {
        char label[80];
        snprintf(label, sizeof(label), "%s (%dx%d)",
                 app->sprites[i].name, app->sprites[i].width, app->sprites[i].height);
        if (ui_selectable(c.x, y, c.w, label, app->current_sprite == i)) {
            app->current_sprite = i;
            app->animate = false;
        }
        y += ui_line_height() + 2;
        if (y > c.y + c.h - ui_line_height()) break;
    }

    ui_panel_end();
}
