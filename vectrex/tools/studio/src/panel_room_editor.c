/*
 * panel_room_editor.c — Room editor canvas
 *
 * Displays cave walls (from row types), enemies, walls, miner, player start, lava.
 * Supports drag-and-drop placement of elements.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

/* Colors for room elements */
static const SDL_Color COL_CAVE     = {  0, 200,   0, 255};
static const SDL_Color COL_WALL     = {200, 200,   0, 255};
static const SDL_Color COL_WALL_D   = {200, 100,   0, 255};
static const SDL_Color COL_ENEMY    = {200,   0,   0, 255};
static const SDL_Color COL_MINER    = {  0, 200, 200, 255};
static const SDL_Color COL_PLAYER   = {255, 255, 255, 255};
static const SDL_Color COL_LAVA     = {200,  50,  50, 255};
static const SDL_Color COL_EXIT     = {100, 100, 200, 255};
static const SDL_Color COL_BOUNDARY = {  0, 150, 150, 100};
static const SDL_Color COL_GRID     = { 40,  40,  40, 255};

void draw_room_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar: current tool display */
    const char *tool_names[] = {"Select", "Wall", "Bat", "Spider", "Snake", "Miner", "Player"};
    char tool_label[32];
    snprintf(tool_label, sizeof(tool_label), "Tool: %s", tool_names[app->room_tool]);
    ui_text_color(tb.x, tb.y + 2, tool_label, ui_theme.text_dim);

    /* Get current room */
    if (app->cur_level < 0 || app->cur_level >= app->project.level_count) { ui_panel_end(); return; }
    Level *lvl = &app->project.levels[app->cur_level];
    if (app->cur_room < 0 || app->cur_room >= lvl->room_count) { ui_panel_end(); return; }
    Room *room = &lvl->rooms[app->cur_room];

    /* Canvas area with margin */
    int margin = 10;
    int canvas_x = c.x + margin;
    int canvas_y = c.y + margin;
    int canvas_w = c.w - margin * 2;
    int canvas_h = c.h - margin * 2;

    /* Black background */
    SDL_Rect bg = {canvas_x, canvas_y, canvas_w, canvas_h};
    SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    /* Grid */
    SDL_SetRenderDrawColor(app->renderer, COL_GRID.r, COL_GRID.g, COL_GRID.b, 255);
    for (int vx = ROOM_LEFT; vx <= ROOM_RIGHT; vx += 25) {
        int px1, py1;
        room_vx_to_px(vx, ROOM_TOP, canvas_x, canvas_y, canvas_w, canvas_h, &px1, &py1);
        int px2, py2;
        room_vx_to_px(vx, ROOM_BOTTOM, canvas_x, canvas_y, canvas_w, canvas_h, &px2, &py2);
        SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
    }
    for (int vy = ROOM_BOTTOM; vy <= ROOM_TOP; vy += 10) {
        int px1, py1, px2, py2;
        room_vx_to_px(ROOM_LEFT, vy, canvas_x, canvas_y, canvas_w, canvas_h, &px1, &py1);
        room_vx_to_px(ROOM_RIGHT, vy, canvas_x, canvas_y, canvas_w, canvas_h, &px2, &py2);
        SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
    }

    /* Row boundaries */
    SDL_SetRenderDrawColor(app->renderer, COL_BOUNDARY.r, COL_BOUNDARY.g, COL_BOUNDARY.b, 255);
    int bx1, by1, bx2, by2;
    room_vx_to_px(ROOM_LEFT, ROW_BOUNDARY_TOP, canvas_x, canvas_y, canvas_w, canvas_h, &bx1, &by1);
    room_vx_to_px(ROOM_RIGHT, ROW_BOUNDARY_TOP, canvas_x, canvas_y, canvas_w, canvas_h, &bx2, &by2);
    SDL_RenderDrawLine(app->renderer, bx1, by1, bx2, by2);
    room_vx_to_px(ROOM_LEFT, ROW_BOUNDARY_MID, canvas_x, canvas_y, canvas_w, canvas_h, &bx1, &by1);
    room_vx_to_px(ROOM_RIGHT, ROW_BOUNDARY_MID, canvas_x, canvas_y, canvas_w, canvas_h, &bx2, &by2);
    SDL_RenderDrawLine(app->renderer, bx1, by1, bx2, by2);

    /* Draw cave polylines from row types */
    SDL_SetRenderDrawColor(app->renderer, COL_CAVE.r, COL_CAVE.g, COL_CAVE.b, 255);
    int row_y_offsets[] = {ROOM_TOP, ROW_BOUNDARY_TOP, ROW_BOUNDARY_MID};
    for (int ri = 0; ri < 3; ri++) {
        int rt_idx = room->rows[ri];
        if (rt_idx < 0 || rt_idx >= app->project.row_type_count) continue;
        RowType *rt = &app->project.row_types[rt_idx];
        int y_off = row_y_offsets[ri];

        for (int pi = 0; pi < rt->cave_line_count; pi++) {
            Polyline *pl = &rt->cave_lines[pi];
            for (int j = 1; j < pl->count; j++) {
                int px1, py1, px2, py2;
                /* Convert row-local coords to room coords */
                int vx1 = pl->points[j-1].x;
                int vy1 = y_off - (ROW_TOP - pl->points[j-1].y);
                int vx2 = pl->points[j].x;
                int vy2 = y_off - (ROW_TOP - pl->points[j].y);
                room_vx_to_px(vx1, vy1, canvas_x, canvas_y, canvas_w, canvas_h, &px1, &py1);
                room_vx_to_px(vx2, vy2, canvas_x, canvas_y, canvas_w, canvas_h, &px2, &py2);
                SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
            }
        }
    }

    /* Draw walls */
    for (int i = 0; i < room->wall_count; i++) {
        Wall *w = &room->walls[i];
        int wx1, wy1, wx2, wy2;
        room_vx_to_px(w->x, w->y, canvas_x, canvas_y, canvas_w, canvas_h, &wx1, &wy1);
        room_vx_to_px(w->x + w->w, w->y - w->h, canvas_x, canvas_y, canvas_w, canvas_h, &wx2, &wy2);
        SDL_Color wc = w->destroyable ? COL_WALL_D : COL_WALL;
        SDL_SetRenderDrawColor(app->renderer, wc.r, wc.g, wc.b, 255);
        SDL_Rect wr = {wx1, wy1, wx2 - wx1, wy2 - wy1};
        SDL_RenderFillRect(app->renderer, &wr);
    }

    /* Draw enemies */
    SDL_SetRenderDrawColor(app->renderer, COL_ENEMY.r, COL_ENEMY.g, COL_ENEMY.b, 255);
    for (int i = 0; i < room->enemy_count; i++) {
        Enemy *e = &room->enemies[i];
        int ex, ey;
        room_vx_to_px(e->x, e->y, canvas_x, canvas_y, canvas_w, canvas_h, &ex, &ey);
        SDL_Rect er = {ex - 4, ey - 4, 8, 8};
        SDL_RenderFillRect(app->renderer, &er);
        /* Type label */
        const char *etype = e->type == ENEMY_BAT ? "B" : e->type == ENEMY_SPIDER ? "S" : "N";
        ui_text_mono_color(ex + 6, ey - 6, etype, COL_ENEMY);
    }

    /* Draw miner */
    if (room->has_miner) {
        int mx, my;
        room_vx_to_px(room->miner.x, room->miner.y, canvas_x, canvas_y, canvas_w, canvas_h, &mx, &my);
        SDL_SetRenderDrawColor(app->renderer, COL_MINER.r, COL_MINER.g, COL_MINER.b, 255);
        SDL_Rect mr = {mx - 5, my - 5, 10, 10};
        SDL_RenderDrawRect(app->renderer, &mr);
        ui_text_mono_color(mx + 7, my - 6, "M", COL_MINER);
    }

    /* Draw player start */
    if (room->has_player_start) {
        int sx, sy;
        room_vx_to_px(room->player_start.x, room->player_start.y,
                       canvas_x, canvas_y, canvas_w, canvas_h, &sx, &sy);
        SDL_SetRenderDrawColor(app->renderer, COL_PLAYER.r, COL_PLAYER.g, COL_PLAYER.b, 255);
        /* Draw cross */
        SDL_RenderDrawLine(app->renderer, sx - 6, sy, sx + 6, sy);
        SDL_RenderDrawLine(app->renderer, sx, sy - 6, sx, sy + 6);
        ui_text_mono_color(sx + 8, sy - 6, "P", COL_PLAYER);
    }

    /* Draw lava zone */
    if (room->has_lava) {
        int lx1, ly1, lx2, ly2;
        room_vx_to_px(ROOM_LEFT, -40, canvas_x, canvas_y, canvas_w, canvas_h, &lx1, &ly1);
        room_vx_to_px(ROOM_RIGHT, ROOM_BOTTOM, canvas_x, canvas_y, canvas_w, canvas_h, &lx2, &ly2);
        SDL_SetRenderDrawColor(app->renderer, COL_LAVA.r, COL_LAVA.g, COL_LAVA.b, 80);
        SDL_Rect lr = {lx1, ly1, lx2 - lx1, ly2 - ly1};
        SDL_RenderFillRect(app->renderer, &lr);
    }

    /* Mouse coordinate display */
    if (ui_mouse_in_rect(canvas_x, canvas_y, canvas_w, canvas_h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int vx, vy;
        room_px_to_vx(mx, my, canvas_x, canvas_y, canvas_w, canvas_h, &vx, &vy);
        char coord[32];
        snprintf(coord, sizeof(coord), "(%d, %d)", vx, vy);
        ui_tooltip(coord);

        /* Click to place element with current tool */
        if (ui_mouse_clicked() && app->room_tool != TOOL_SELECT) {
            switch (app->room_tool) {
            case TOOL_WALL:
                if (room->wall_count < MAX_WALLS) {
                    room->walls[room->wall_count++] = (Wall){vy, vx, 10, 20, false};
                    app->modified = true;
                }
                break;
            case TOOL_ENEMY_BAT: case TOOL_ENEMY_SPIDER: case TOOL_ENEMY_SNAKE:
                if (room->enemy_count < MAX_ENEMIES) {
                    int type = app->room_tool - TOOL_ENEMY_BAT;
                    room->enemies[room->enemy_count++] = (Enemy){vx, vy, 1, type};
                    app->modified = true;
                }
                break;
            case TOOL_MINER:
                room->miner = (Point){vx, vy};
                room->has_miner = true;
                app->modified = true;
                break;
            case TOOL_PLAYER_START:
                room->player_start = (Point){vx, vy};
                room->has_player_start = true;
                app->modified = true;
                break;
            default: break;
            }
        }
    }

    ui_panel_end();
}
