/*
 * panel_level_list.c — Level and room list (left panel)
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_level_list(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Levels", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    Project *proj = &app->project;

    for (int li = 0; li < proj->level_count; li++) {
        Level *lvl = &proj->levels[li];

        /* Level header */
        bool lvl_sel = (app->cur_level == li);
        char label[80];
        snprintf(label, sizeof(label), "%s (%d rooms)", lvl->name, lvl->room_count);
        if (ui_selectable(c.x, y, c.w, label, lvl_sel && app->cur_room < 0)) {
            app->cur_level = li;
            app->cur_room = lvl->room_count > 0 ? 0 : -1;
        }
        y += ui_line_height() + 2;

        /* Room list under selected level */
        if (lvl_sel) {
            for (int ri = 0; ri < lvl->room_count; ri++) {
                snprintf(label, sizeof(label), "  Room %d", lvl->rooms[ri].number);
                if (ui_selectable(c.x, y, c.w, label, app->cur_room == ri)) {
                    app->cur_room = ri;
                }
                y += ui_line_height() + 1;
            }
        }

        if (y > c.y + c.h - ui_line_height()) break;
    }

    ui_panel_end();
}
