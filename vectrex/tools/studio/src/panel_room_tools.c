/*
 * panel_room_tools.c — Room editing tools (right panel)
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_room_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* Tools section */
    y = ui_section(c.x - 4, y, c.w + 8, "Tools");

    int bw = (c.w - 4) / 2;
    int bh = ui_line_height() + 4;

    if (ui_button(c.x, y, bw, bh, "Select"))  app->room_tool = TOOL_SELECT;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Wall"))  app->room_tool = TOOL_WALL;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Bat"))     app->room_tool = TOOL_ENEMY_BAT;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Spider")) app->room_tool = TOOL_ENEMY_SPIDER;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Snake"))   app->room_tool = TOOL_ENEMY_SNAKE;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Miner"))  app->room_tool = TOOL_MINER;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Player"))  app->room_tool = TOOL_PLAYER_START;
    y += bh + 10;

    /* Keyboard shortcuts */
    if (ui_key_pressed(SDLK_s)) app->room_tool = TOOL_SELECT;
    if (ui_key_pressed(SDLK_w)) app->room_tool = TOOL_WALL;
    if (ui_key_pressed(SDLK_b)) app->room_tool = TOOL_ENEMY_BAT;
    if (ui_key_pressed(SDLK_i)) app->room_tool = TOOL_ENEMY_SPIDER;
    if (ui_key_pressed(SDLK_k)) app->room_tool = TOOL_ENEMY_SNAKE;
    if (ui_key_pressed(SDLK_m)) app->room_tool = TOOL_MINER;
    if (ui_key_pressed(SDLK_p)) app->room_tool = TOOL_PLAYER_START;

    /* Room properties */
    if (app->cur_level >= 0 && app->cur_level < app->project.level_count &&
        app->cur_room >= 0) {
        Level *lvl = &app->project.levels[app->cur_level];
        if (app->cur_room < lvl->room_count) {
            Room *room = &lvl->rooms[app->cur_room];

            y = ui_section(c.x - 4, y, c.w + 8, "Room Properties");

            /* Row type selectors */
            const char *row_labels[] = {"Top:", "Mid:", "Bot:"};
            for (int i = 0; i < 3; i++) {
                int lw = ui_text_width("Top:");
                ui_text_color(c.x, y + 3, row_labels[i], ui_theme.text_dim);
                if (ui_input_int_ex(c.x, y, c.w, row_labels[i], lw,
                                     &room->rows[i], 1, 0, MAX_ROW_TYPES - 1)) {
                    app->modified = true;
                }
                y += ui_line_height() + 8;
            }

            /* Lava toggle */
            if (ui_checkbox(c.x, y, "Has Lava", &room->has_lava))
                app->modified = true;
            y += ui_line_height() + 8;

            /* Info */
            char info[64];
            snprintf(info, sizeof(info), "Walls: %d  Enemies: %d", room->wall_count, room->enemy_count);
            ui_text_color(c.x, y, info, ui_theme.text_dark);
        }
    }

    ui_panel_end();
}
