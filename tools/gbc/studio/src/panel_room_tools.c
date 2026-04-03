/*
 * panel_room_tools.c — Room editing tools (right panel)
 *
 * Tool selector, move/snap functions, row type assignment.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_room_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;

    /* ── Tool selector ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Place Tool");

    int bw = (c.w - 4) / 2;
    int bh = ui_line_height() + 4;

    if (ui_button(c.x, y, bw, bh, "Select"))  app->room_tool = TOOL_SELECT;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Wall"))    app->room_tool = TOOL_WALL;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Bat"))     app->room_tool = TOOL_ENEMY_BAT;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Spider"))  app->room_tool = TOOL_ENEMY_SPIDER;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Snake"))   app->room_tool = TOOL_ENEMY_SNAKE;
    if (ui_button(c.x + bw + 4, y, bw, bh, "Miner"))   app->room_tool = TOOL_MINER;
    y += bh + 4;
    if (ui_button(c.x, y, bw, bh, "Player"))  app->room_tool = TOOL_PLAYER_START;
    y += bh + 10;

    /* Keyboard shortcuts */
    if (ui_key_pressed(SDLK_s)) app->room_tool = TOOL_SELECT;
    if (ui_key_pressed(SDLK_w)) app->room_tool = TOOL_WALL;
    if (ui_key_pressed(SDLK_e)) app->room_tool = TOOL_ENEMY_BAT;
    if (ui_key_pressed(SDLK_i)) app->room_tool = TOOL_ENEMY_SPIDER;
    if (ui_key_pressed(SDLK_k)) app->room_tool = TOOL_ENEMY_SNAKE;
    if (ui_key_pressed(SDLK_m)) app->room_tool = TOOL_MINER;
    if (ui_key_pressed(SDLK_p)) app->room_tool = TOOL_PLAYER_START;

    /* ── Row types ── */
    if (app->cur_level >= 0 && app->cur_level < app->level_project.level_count) {
        Level *lvl = &app->level_project.levels[app->cur_level];
        if (app->cur_room >= 0 && app->cur_room < lvl->room_count) {
            Room *room = &lvl->rooms[app->cur_room];

            y = ui_section(c.x - 4, y, c.w + 8, "Row Types");

            const char *row_labels[] = {"Top:", "Mid:", "Bot:"};
            int lw = ui_text_width("Top:");
            for (int i = 0; i < 3; i++) {
                if (ui_input_int_ex(c.x, y, c.w, row_labels[i], lw,
                                     &room->rows[i], 1, 0, MAX_ROW_TYPES - 1))
                    app->modified = true;
                y += ui_line_height() + 8;
            }

            /* ── Info ── */
            y = ui_section(c.x - 4, y, c.w + 8, "Info");

            char info[64];
            snprintf(info, sizeof(info), "Walls: %d / %d", room->wall_count, MAX_WALLS);
            ui_text_color(c.x, y, info, ui_theme.text_dim);
            y += ui_line_height() + 2;
            snprintf(info, sizeof(info), "Enemies: %d / %d", room->enemy_count, MAX_ENEMIES);
            ui_text_color(c.x, y, info, ui_theme.text_dim);
            y += ui_line_height() + 2;
            ui_text_color(c.x, y, room->has_miner ? "Miner: yes" : "Miner: no", ui_theme.text_dim);
            y += ui_line_height() + 2;
            ui_text_color(c.x, y, room->has_player_start ? "Player: yes" : "Player: no", ui_theme.text_dim);
        }
    }

    ui_panel_end();
}
