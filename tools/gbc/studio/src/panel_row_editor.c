/*
 * panel_row_editor.c — Cave polyline editor for row types
 *
 * Row-local coords: X: -125..125, Y: 0..30
 * Click in empty space to start drawing a polyline.
 * Right-click to finish. Click on a point to select/drag. Delete to remove.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

static void row_vx_to_px(int vx, int vy, int cx, int cy, int cw, int ch, int *px, int *py) {
    *px = cx + (int)((float)(vx - ROW_LEFT) / 250.0f * cw);
    *py = cy + (int)((float)(ROW_TOP - vy) / 30.0f * ch);
}

static void row_px_to_vx(int px, int py, int cx, int cy, int cw, int ch, int *vx, int *vy, bool snap) {
    *vx = ROW_LEFT + (int)((float)(px - cx) / cw * 250.0f);
    *vy = ROW_TOP - (int)((float)(py - cy) / ch * 30.0f);
    if (snap) { *vx = ((*vx + 2) / 5) * 5; *vy = ((*vy + 2) / 5) * 5; }
    *vx = clamp_i(*vx, ROW_LEFT, ROW_RIGHT);
    *vy = clamp_i(*vy, ROW_BOTTOM, ROW_TOP);
}

static int sel_polyline = -1, sel_point = -1;
static bool snap_enabled = true;

void draw_row_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar info */
    char info[64];
    snprintf(info, sizeof(info), "Row Type %d  |  %s",
             app->cur_row_type, app->drawing_polyline ? "Drawing..." : "Select");
    ui_text_color(tb.x, tb.y + 2, info, ui_theme.text_dim);

    int bx = tb.x + tb.w - 160;
    if (ui_button(bx, tb.y, 50, tb.h, "Clear")) {
        app->level_project.row_types[app->cur_row_type].cave_line_count = 0;
        app->modified = true;
    }
    bx += 54;
    ui_checkbox(bx, tb.y + 2, "Snap", &snap_enabled);

    RowType *rt = &app->level_project.row_types[app->cur_row_type];

    /* Canvas */
    int margin = 10;
    int canvas_x = c.x + margin, canvas_y = c.y + margin;
    int canvas_w = c.w - margin * 2, canvas_h = c.h - margin * 2;

    SDL_Rect bg = {canvas_x, canvas_y, canvas_w, canvas_h};
    SDL_SetRenderDrawColor(app->renderer, 10, 10, 26, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    /* Grid */
    SDL_SetRenderDrawColor(app->renderer, 26, 26, 46, 255);
    for (int vx = ROW_LEFT; vx <= ROW_RIGHT; vx += 5) {
        int gx1, gy1, gx2, gy2;
        row_vx_to_px(vx, ROW_TOP, canvas_x, canvas_y, canvas_w, canvas_h, &gx1, &gy1);
        row_vx_to_px(vx, ROW_BOTTOM, canvas_x, canvas_y, canvas_w, canvas_h, &gx2, &gy2);
        SDL_RenderDrawLine(app->renderer, gx1, gy1, gx2, gy2);
    }
    /* Center line */
    int cx1, cy1, cx2, cy2;
    row_vx_to_px(0, ROW_TOP, canvas_x, canvas_y, canvas_w, canvas_h, &cx1, &cy1);
    row_vx_to_px(0, ROW_BOTTOM, canvas_x, canvas_y, canvas_w, canvas_h, &cx2, &cy2);
    SDL_SetRenderDrawColor(app->renderer, 0, 200, 200, 255);
    SDL_RenderDrawLine(app->renderer, cx1, cy1, cx2, cy2);
    SDL_SetRenderDrawColor(app->renderer, 42, 42, 62, 255);
    SDL_RenderDrawRect(app->renderer, &bg);

    /* Draw cave polylines */
    for (int pi = 0; pi < rt->cave_line_count; pi++) {
        Polyline *pl = &rt->cave_lines[pi];
        SDL_SetRenderDrawColor(app->renderer, 220, 220, 255, 255);
        for (int j = 1; j < pl->count; j++) {
            int px1, py1, px2, py2;
            row_vx_to_px(pl->points[j-1].x, pl->points[j-1].y, canvas_x, canvas_y, canvas_w, canvas_h, &px1, &py1);
            row_vx_to_px(pl->points[j].x, pl->points[j].y, canvas_x, canvas_y, canvas_w, canvas_h, &px2, &py2);
            SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
        }
        for (int j = 0; j < pl->count; j++) {
            int hx, hy;
            row_vx_to_px(pl->points[j].x, pl->points[j].y, canvas_x, canvas_y, canvas_w, canvas_h, &hx, &hy);
            bool sel = (sel_polyline == pi && sel_point == j);
            SDL_Color hc = sel ? (SDL_Color){255,255,0,255} : (SDL_Color){220,220,255,255};
            SDL_SetRenderDrawColor(app->renderer, hc.r, hc.g, hc.b, 255);
            SDL_Rect hr = {hx-3, hy-3, 6, 6};
            SDL_RenderFillRect(app->renderer, &hr);
        }
    }

    /* In-progress polyline */
    if (app->drawing_polyline && app->temp_polyline.count > 0) {
        SDL_SetRenderDrawColor(app->renderer, 220, 220, 255, 255);
        for (int j = 0; j < app->temp_polyline.count; j++) {
            int dx, dy;
            row_vx_to_px(app->temp_polyline.points[j].x, app->temp_polyline.points[j].y,
                         canvas_x, canvas_y, canvas_w, canvas_h, &dx, &dy);
            if (j > 0) {
                int px1, py1;
                row_vx_to_px(app->temp_polyline.points[j-1].x, app->temp_polyline.points[j-1].y,
                             canvas_x, canvas_y, canvas_w, canvas_h, &px1, &py1);
                SDL_RenderDrawLine(app->renderer, px1, py1, dx, dy);
            }
            SDL_Rect dr = {dx-3, dy-3, 6, 6};
            SDL_RenderFillRect(app->renderer, &dr);
        }
    }

    /* Mouse */
    if (ui_mouse_in_rect(canvas_x, canvas_y, canvas_w, canvas_h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int vx, vy;
        row_px_to_vx(mx, my, canvas_x, canvas_y, canvas_w, canvas_h, &vx, &vy, snap_enabled);
        char coord[32];
        snprintf(coord, sizeof(coord), "(%d, %d)", vx, vy);
        ui_tooltip(coord);

        if (app->drawing_polyline) {
            if (ui_mouse_clicked() && app->temp_polyline.count < MAX_POINTS)
                app->temp_polyline.points[app->temp_polyline.count++] = (Point){vx, vy};
            if (ui_mouse_right_clicked()) {
                if (app->temp_polyline.count >= 2 && rt->cave_line_count < MAX_POLYLINES) {
                    rt->cave_lines[rt->cave_line_count++] = app->temp_polyline;
                    app->modified = true;
                }
                app->temp_polyline.count = 0;
                app->drawing_polyline = false;
            }
        } else {
            if (ui_mouse_clicked()) {
                sel_polyline = -1; sel_point = -1;
                bool hit = false;
                for (int pi = 0; pi < rt->cave_line_count && !hit; pi++) {
                    Polyline *pl = &rt->cave_lines[pi];
                    for (int j = 0; j < pl->count; j++) {
                        int hx, hy;
                        row_vx_to_px(pl->points[j].x, pl->points[j].y,
                                     canvas_x, canvas_y, canvas_w, canvas_h, &hx, &hy);
                        if (abs(mx - hx) < 8 && abs(my - hy) < 8) {
                            sel_polyline = pi; sel_point = j; hit = true; break;
                        }
                    }
                }
                if (!hit) {
                    app->drawing_polyline = true;
                    app->temp_polyline.count = 1;
                    app->temp_polyline.points[0] = (Point){vx, vy};
                }
            }
            if (sel_polyline >= 0 && sel_point >= 0 && ui_mouse_down()) {
                rt->cave_lines[sel_polyline].points[sel_point] = (Point){vx, vy};
                app->modified = true;
            }
        }
    }

    /* Delete */
    if (sel_polyline >= 0 && sel_point >= 0 &&
        (ui_key_pressed(SDLK_DELETE) || ui_key_pressed(SDLK_BACKSPACE))) {
        Polyline *pl = &rt->cave_lines[sel_polyline];
        for (int j = sel_point; j < pl->count - 1; j++)
            pl->points[j] = pl->points[j + 1];
        pl->count--;
        if (pl->count < 2) {
            for (int j = sel_polyline; j < rt->cave_line_count - 1; j++)
                rt->cave_lines[j] = rt->cave_lines[j + 1];
            rt->cave_line_count--;
        }
        sel_polyline = -1; sel_point = -1;
        app->modified = true;
    }

    if (app->drawing_polyline && ui_key_pressed(SDLK_ESCAPE)) {
        app->temp_polyline.count = 0;
        app->drawing_polyline = false;
    }

    ui_panel_end();
}
