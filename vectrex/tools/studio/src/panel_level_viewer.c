/*
 * panel_level_viewer.c — Level overview: all rooms in a grid with exits
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>

/* Colors */
static const SDL_Color COL_ROOM_BG   = { 30,  30,  40, 255};
static const SDL_Color COL_ROOM_SEL  = { 50,  70, 100, 255};
static const SDL_Color COL_ROOM_BDR  = { 80,  80, 100, 255};
static const SDL_Color COL_EXIT_LINE = {100, 150, 200, 255};
static const SDL_Color COL_CAVE_LINE = {  0, 150,   0, 255};
static const SDL_Color COL_WALL_MINI = {200, 200,   0, 255};
static const SDL_Color COL_ENEMY_MINI= {200,   0,   0, 255};
static const SDL_Color COL_LAVA_MINI = {150,  30,  30, 255};

void draw_level_viewer(App *app, int x, int y, int w, int h) {
    ui_panel_begin("Level Viewer", x, y, w, h);
    SDL_Rect c = ui_panel_content();

    if (app->cur_level < 0 || app->cur_level >= app->project.level_count) {
        ui_panel_end(); return;
    }
    Level *lvl = &app->project.levels[app->cur_level];

    /* Calculate grid layout */
    int cols = 4;
    if (lvl->room_count <= 2) cols = 2;
    else if (lvl->room_count <= 6) cols = 3;
    int rows_needed = (lvl->room_count + cols - 1) / cols;

    int gap = 8;
    int room_w = (c.w - gap * (cols + 1)) / cols;
    int room_h = (c.h - gap * (rows_needed + 1)) / rows_needed;
    if (room_h > room_w / 2) room_h = room_w / 2;

    for (int i = 0; i < lvl->room_count; i++) {
        Room *room = &lvl->rooms[i];
        int col = i % cols;
        int row = i / cols;
        int rx = c.x + gap + col * (room_w + gap);
        int ry = c.y + gap + row * (room_h + gap);

        /* Room background */
        SDL_Color bg_col = (app->cur_room == i) ? COL_ROOM_SEL : COL_ROOM_BG;
        SDL_Rect rr = {rx, ry, room_w, room_h};
        SDL_SetRenderDrawColor(app->renderer, bg_col.r, bg_col.g, bg_col.b, 255);
        SDL_RenderFillRect(app->renderer, &rr);
        SDL_SetRenderDrawColor(app->renderer, COL_ROOM_BDR.r, COL_ROOM_BDR.g, COL_ROOM_BDR.b, 255);
        SDL_RenderDrawRect(app->renderer, &rr);

        /* Mini cave lines */
        SDL_SetRenderDrawColor(app->renderer, COL_CAVE_LINE.r, COL_CAVE_LINE.g, COL_CAVE_LINE.b, 255);
        int row_y_offsets[] = {ROOM_TOP, ROW_BOUNDARY_TOP, ROW_BOUNDARY_MID};
        for (int ri = 0; ri < 3; ri++) {
            int rt_idx = room->rows[ri];
            if (rt_idx < 0 || rt_idx >= app->project.row_type_count) continue;
            RowType *rt = &app->project.row_types[rt_idx];
            int y_off = row_y_offsets[ri];
            for (int pi = 0; pi < rt->cave_line_count; pi++) {
                Polyline *pl = &rt->cave_lines[pi];
                for (int j = 1; j < pl->count; j++) {
                    int vx1 = pl->points[j-1].x, vy1 = y_off - (ROW_TOP - pl->points[j-1].y);
                    int vx2 = pl->points[j].x,   vy2 = y_off - (ROW_TOP - pl->points[j].y);
                    int px1 = rx + (int)((float)(vx1 - ROOM_LEFT) / 250.0f * room_w);
                    int py1 = ry + (int)((float)(ROOM_TOP - vy1) / 100.0f * room_h);
                    int px2 = rx + (int)((float)(vx2 - ROOM_LEFT) / 250.0f * room_w);
                    int py2 = ry + (int)((float)(ROOM_TOP - vy2) / 100.0f * room_h);
                    SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
                }
            }
        }

        /* Mini walls */
        SDL_SetRenderDrawColor(app->renderer, COL_WALL_MINI.r, COL_WALL_MINI.g, COL_WALL_MINI.b, 255);
        for (int wi = 0; wi < room->wall_count; wi++) {
            Wall *wl = &room->walls[wi];
            int wx = rx + (int)((float)(wl->x - ROOM_LEFT) / 250.0f * room_w);
            int wy = ry + (int)((float)(ROOM_TOP - wl->y) / 100.0f * room_h);
            int ww = (int)((float)wl->w / 250.0f * room_w);
            int wh = (int)((float)wl->h / 100.0f * room_h);
            if (ww < 2) ww = 2; if (wh < 2) wh = 2;
            SDL_Rect wr = {wx, wy, ww, wh};
            SDL_RenderFillRect(app->renderer, &wr);
        }

        /* Mini enemies */
        SDL_SetRenderDrawColor(app->renderer, COL_ENEMY_MINI.r, COL_ENEMY_MINI.g, COL_ENEMY_MINI.b, 255);
        for (int ei = 0; ei < room->enemy_count; ei++) {
            int ex = rx + (int)((float)(room->enemies[ei].x - ROOM_LEFT) / 250.0f * room_w);
            int ey = ry + (int)((float)(ROOM_TOP - room->enemies[ei].y) / 100.0f * room_h);
            SDL_Rect er = {ex - 2, ey - 2, 4, 4};
            SDL_RenderFillRect(app->renderer, &er);
        }

        /* Lava strip */
        if (room->has_lava) {
            int lava_y = ry + (int)(0.9f * room_h);
            SDL_Rect lr = {rx, lava_y, room_w, room_h - (lava_y - ry)};
            SDL_SetRenderDrawColor(app->renderer, COL_LAVA_MINI.r, COL_LAVA_MINI.g, COL_LAVA_MINI.b, 180);
            SDL_RenderFillRect(app->renderer, &lr);
        }

        /* Room label */
        char label[16];
        snprintf(label, sizeof(label), "R%d", room->number);
        ui_text_color(rx + 3, ry + 2, label, ui_theme.text_dim);

        /* Click to select room and jump to editor */
        if (ui_mouse_in_rect(rx, ry, room_w, room_h) && ui_mouse_clicked()) {
            app->cur_room = i;
            app->view = VIEW_ROOM_EDITOR;
        }
    }

    /* Draw exit connections */
    SDL_SetRenderDrawColor(app->renderer, COL_EXIT_LINE.r, COL_EXIT_LINE.g, COL_EXIT_LINE.b, 255);
    for (int i = 0; i < lvl->room_count; i++) {
        Room *room = &lvl->rooms[i];
        int col1 = i % cols, row1 = i / cols;
        int cx1 = c.x + gap + col1 * (room_w + gap) + room_w / 2;
        int cy1 = c.y + gap + row1 * (room_h + gap) + room_h / 2;

        int exits[] = {room->exit_right, room->exit_bottom}; /* only draw right+down to avoid doubles */
        for (int e = 0; e < 2; e++) {
            if (exits[e] < 0) continue;
            for (int j = 0; j < lvl->room_count; j++) {
                if (lvl->rooms[j].number == exits[e] && j != i) {
                    int col2 = j % cols, row2 = j / cols;
                    int cx2 = c.x + gap + col2 * (room_w + gap) + room_w / 2;
                    int cy2 = c.y + gap + row2 * (room_h + gap) + room_h / 2;
                    SDL_RenderDrawLine(app->renderer, cx1, cy1, cx2, cy2);
                    break;
                }
            }
        }
    }

    ui_panel_end();
}
