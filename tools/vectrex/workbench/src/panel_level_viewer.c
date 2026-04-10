/*
 * panel_level_viewer.c — Level map: rooms placed spatially using exit connections
 *
 * BFS from room 0 using exits to compute a 2D grid layout.
 * Each room drawn at correct position showing cave lines, walls, enemies, etc.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#define DRAW_W 250  /* room width in pixels (unscaled) */
#define DRAW_H 100  /* room height in pixels (unscaled) */

/* Map room-local vectrex coords to pixel coords within a room cell */
static void vcx(int vx, int vy, int ox, int oy, float scale, int *px, int *py) {
    *px = ox + (int)(((vx + 125) / 250.0f) * DRAW_W * scale);
    *py = oy + (int)(((50 - vy) / 100.0f) * DRAW_H * scale);
}

/* BFS to compute grid positions from room exits */
static void compute_layout(Level *lvl, int *gx, int *gy, int *placed) {
    memset(placed, 0, sizeof(int) * MAX_ROOMS);
    memset(gx, 0, sizeof(int) * MAX_ROOMS);
    memset(gy, 0, sizeof(int) * MAX_ROOMS);

    if (lvl->room_count == 0) return;

    placed[0] = 1;
    gx[0] = 0; gy[0] = 0;

    /* Simple BFS queue */
    int queue[MAX_ROOMS], qh = 0, qt = 0;
    queue[qt++] = 0;

    while (qh < qt) {
        int ri = queue[qh++];
        Room *room = &lvl->rooms[ri];
        int cx = gx[ri], cy = gy[ri];

        /* For each exit, find target room index (number is 1-based) */
        struct { int target; int dx; int dy; } exits[] = {
            {room->exit_right,  1,  0},
            {room->exit_left,  -1,  0},
            {room->exit_top,    0, -1},
            {room->exit_bottom, 0,  1},
        };

        for (int e = 0; e < 4; e++) {
            if (exits[e].target < 0) continue;
            int ti = exits[e].target - 1; /* 1-indexed to 0-indexed */
            if (ti < 0 || ti >= lvl->room_count) continue;
            if (placed[ti]) continue;
            placed[ti] = 1;
            gx[ti] = cx + exits[e].dx;
            gy[ti] = cy + exits[e].dy;
            queue[qt++] = ti;
        }
    }

    /* Place unreachable rooms in an extra row */
    int max_gy = 0;
    for (int i = 0; i < lvl->room_count; i++)
        if (placed[i] && gy[i] > max_gy) max_gy = gy[i];

    int col = 0;
    for (int i = 0; i < lvl->room_count; i++) {
        if (!placed[i]) {
            placed[i] = 1;
            gx[i] = col++;
            gy[i] = max_gy + 2;
        }
    }

    /* Normalize so min is (0,0) */
    int min_gx = gx[0], min_gy = gy[0];
    for (int i = 1; i < lvl->room_count; i++) {
        if (gx[i] < min_gx) min_gx = gx[i];
        if (gy[i] < min_gy) min_gy = gy[i];
    }
    for (int i = 0; i < lvl->room_count; i++) {
        gx[i] -= min_gx;
        gy[i] -= min_gy;
    }
}

void draw_level_viewer(App *app, int x, int y, int w, int h) {
    ui_panel_begin("Level Viewer", x, y, w, h);
    SDL_Rect c = ui_panel_content();

    if (app->cur_level < 0 || app->cur_level >= app->project.level_count) {
        ui_panel_end(); return;
    }
    Level *lvl = &app->project.levels[app->cur_level];
    if (lvl->room_count == 0) { ui_panel_end(); return; }

    /* Compute layout */
    int grid_x[MAX_ROOMS], grid_y[MAX_ROOMS], placed[MAX_ROOMS];
    compute_layout(lvl, grid_x, grid_y, placed);

    int max_gx = 0, max_gy = 0;
    for (int i = 0; i < lvl->room_count; i++) {
        if (grid_x[i] > max_gx) max_gx = grid_x[i];
        if (grid_y[i] > max_gy) max_gy = grid_y[i];
    }

    /* Compute scale to fit */
    int cols = max_gx + 1, rows = max_gy + 1;
    float scale_x = (float)c.w / (cols * DRAW_W);
    float scale_y = (float)c.h / (rows * DRAW_H);
    float scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale > 1.5f) scale = 1.5f;

    int cell_w = (int)(DRAW_W * scale);
    int cell_h = (int)(DRAW_H * scale);

    /* Center the map */
    int total_w = cols * cell_w;
    int total_h = rows * cell_h;
    int off_x = c.x + (c.w - total_w) / 2;
    int off_y = c.y + (c.h - total_h) / 2;

    for (int i = 0; i < lvl->room_count; i++) {
        Room *room = &lvl->rooms[i];
        int ox = off_x + grid_x[i] * cell_w;
        int oy = off_y + grid_y[i] * cell_h;

        /* Room border */
        SDL_Color bdr = (app->cur_room == i) ? (SDL_Color){255, 255, 0, 255} : (SDL_Color){50, 50, 70, 255};
        int bw = (app->cur_room == i) ? 2 : 1;
        SDL_Rect rr = {ox, oy, cell_w, cell_h};
        SDL_SetRenderDrawColor(app->renderer, bdr.r, bdr.g, bdr.b, 255);
        for (int b = 0; b < bw; b++) {
            SDL_Rect br = {rr.x + b, rr.y + b, rr.w - b*2, rr.h - b*2};
            SDL_RenderDrawRect(app->renderer, &br);
        }

        /* Cave lines */
        SDL_SetRenderDrawColor(app->renderer, 220, 220, 255, 255);
        int row_y_offsets[] = {20, -10, -40};
        for (int ri = 0; ri < 3; ri++) {
            int rt_idx = room->rows[ri];
            if (rt_idx < 0 || rt_idx >= app->project.row_type_count) continue;
            RowType *rt = &app->project.row_types[rt_idx];
            for (int pi = 0; pi < rt->cave_line_count; pi++) {
                Polyline *pl = &rt->cave_lines[pi];
                for (int j = 1; j < pl->count; j++) {
                    int px1, py1, px2, py2;
                    vcx(pl->points[j-1].x, pl->points[j-1].y + row_y_offsets[ri],
                        ox, oy, scale, &px1, &py1);
                    vcx(pl->points[j].x, pl->points[j].y + row_y_offsets[ri],
                        ox, oy, scale, &px2, &py2);
                    SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
                }
            }
        }

        /* Lava */
        if (room->has_lava) {
            int lx1, ly1, lx2, ly2;
            vcx(-125, -40, ox, oy, scale, &lx1, &ly1);
            vcx(125, -50, ox, oy, scale, &lx2, &ly2);
            SDL_SetRenderDrawColor(app->renderer, 60, 25, 0, 255);
            SDL_Rect lr = {lx1, ly1, lx2 - lx1, ly2 - ly1};
            SDL_RenderFillRect(app->renderer, &lr);
            SDL_SetRenderDrawColor(app->renderer, 255, 100, 0, 255);
            SDL_RenderDrawLine(app->renderer, lx1, ly1, lx2, ly1);
        }

        /* Walls */
        for (int wi = 0; wi < room->wall_count; wi++) {
            Wall *wl = &room->walls[wi];
            int wx1, wy1, wx2, wy2;
            vcx(wl->x, wl->y, ox, oy, scale, &wx1, &wy1);
            vcx(wl->x + wl->w, wl->y - wl->h, ox, oy, scale, &wx2, &wy2);
            SDL_Color wc = wl->destroyable ? (SDL_Color){255,100,70,255} : (SDL_Color){130,170,255,255};
            SDL_SetRenderDrawColor(app->renderer, wc.r, wc.g, wc.b, 255);
            SDL_Rect wr = {wx1, wy1, wx2 - wx1, wy2 - wy1};
            SDL_RenderDrawRect(app->renderer, &wr);
        }

        /* Enemies */
        for (int ei = 0; ei < room->enemy_count; ei++) {
            Enemy *e = &room->enemies[ei];
            int epx, epy;
            vcx(e->x, e->y, ox, oy, scale, &epx, &epy);
            SDL_Color ec = e->type == ENEMY_SPIDER ? (SDL_Color){200,50,255,255} :
                           e->type == ENEMY_SNAKE  ? (SDL_Color){50,200,50,255} :
                                                     (SDL_Color){255,50,50,255};
            SDL_SetRenderDrawColor(app->renderer, ec.r, ec.g, ec.b, 255);
            int r = (int)(3 * scale); if (r < 2) r = 2;
            SDL_Rect er = {epx - r, epy - r, r*2, r*2};
            SDL_RenderDrawRect(app->renderer, &er);
        }

        /* Miner */
        if (room->has_miner) {
            int mx, my;
            vcx(room->miner.x, room->miner.y, ox, oy, scale, &mx, &my);
            SDL_SetRenderDrawColor(app->renderer, 0, 255, 130, 255);
            int r = (int)(3 * scale); if (r < 2) r = 2;
            SDL_Rect mr = {mx - r, my - r, r*2, r*2};
            SDL_RenderDrawRect(app->renderer, &mr);
        }

        /* Player start */
        if (room->has_player_start) {
            int spx, spy;
            vcx(room->player_start.x, room->player_start.y, ox, oy, scale, &spx, &spy);
            SDL_SetRenderDrawColor(app->renderer, 255, 255, 0, 255);
            int r = (int)(4 * scale); if (r < 2) r = 2;
            SDL_RenderDrawLine(app->renderer, spx, spy - r, spx - r, spy + r);
            SDL_RenderDrawLine(app->renderer, spx - r, spy + r, spx + r, spy + r);
            SDL_RenderDrawLine(app->renderer, spx + r, spy + r, spx, spy - r);
        }

        /* Room label */
        char label[16];
        snprintf(label, sizeof(label), "R%d", room->number);
        ui_text_small(ox + 2, oy + 1, label);

        /* Click to select and jump to editor */
        if (ui_mouse_in_rect(ox, oy, cell_w, cell_h) && ui_mouse_clicked()) {
            app->cur_room = i;
            app->view = VIEW_ROOM_EDITOR;
        }
    }

    ui_panel_end();
}
