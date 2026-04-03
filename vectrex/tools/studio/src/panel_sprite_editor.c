/*
 * panel_sprite_editor.c — VLC vector sprite editor
 *
 * Coords: -20..20. Draw mode: click to add points. Select mode: click/drag/delete.
 * Right-click: undo last point. Shows bounding box and VLC data.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

#define SPR_RANGE 40  /* -20..20 */
#define SPR_HALF  20

static int sel_pt = -1;
static bool spr_draw_mode = true;  /* true=draw, false=select */

static void spr_vx_to_px(int vx, int vy, int cx, int cy, int cw, int ch, int *px, int *py) {
    *px = cx + (int)((float)(vx + SPR_HALF) / SPR_RANGE * cw);
    *py = cy + (int)((float)(SPR_HALF - vy) / SPR_RANGE * ch);
}

static void spr_px_to_vx(int px, int py, int cx, int cy, int cw, int ch, int *vx, int *vy) {
    *vx = -SPR_HALF + (int)((float)(px - cx) / cw * SPR_RANGE);
    *vy = SPR_HALF - (int)((float)(py - cy) / ch * SPR_RANGE);
    *vx = clamp_i(*vx, -SPR_HALF, SPR_HALF);
    *vy = clamp_i(*vy, -SPR_HALF, SPR_HALF);
}

static SpriteFrame *get_frame(App *app) {
    if (app->cur_sprite < 0 || app->cur_sprite >= app->project.sprite_count) return NULL;
    VecSprite *spr = &app->project.sprites[app->cur_sprite];
    if (app->cur_frame < 0 || app->cur_frame >= spr->frame_count) return NULL;
    return &spr->frames[app->cur_frame];
}

void draw_sprite_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar */
    char info[64];
    VecSprite *spr = (app->cur_sprite >= 0 && app->cur_sprite < app->project.sprite_count)
                     ? &app->project.sprites[app->cur_sprite] : NULL;
    snprintf(info, sizeof(info), "%s  Frame %d/%d  |  %s",
             spr ? spr->name : "—", app->cur_frame + 1,
             spr ? spr->frame_count : 0,
             spr_draw_mode ? "Draw" : "Select");
    ui_text_color(tb.x, tb.y + 2, info, ui_theme.text_dim);

    /* Toolbar buttons */
    int bx = tb.x + tb.w - 260;
    int bh = tb.h;
    if (ui_button(bx, tb.y, 50, bh, "Draw")) spr_draw_mode = true;
    bx += 54;
    if (ui_button(bx, tb.y, 50, bh, "Select")) spr_draw_mode = false;
    bx += 54;
    if (ui_button(bx, tb.y, 50, bh, "Clear")) {
        SpriteFrame *f = get_frame(app);
        if (f) { f->count = 0; app->modified = true; }
    }
    bx += 54;
    if (ui_button(bx, tb.y, 50, bh, "MirH")) {
        SpriteFrame *f = get_frame(app);
        if (f) { for (int i = 0; i < f->count; i++) f->points[i].x = -f->points[i].x; app->modified = true; }
    }
    bx += 54;
    if (ui_button(bx, tb.y, 50, bh, "MirV")) {
        SpriteFrame *f = get_frame(app);
        if (f) { for (int i = 0; i < f->count; i++) f->points[i].y = -f->points[i].y; app->modified = true; }
    }

    SpriteFrame *frame = get_frame(app);

    /* Canvas — square, centered */
    int canvas_size = c.h < c.w ? c.h : c.w;
    int canvas_x = c.x + (c.w - canvas_size) / 2;
    int canvas_y = c.y + (c.h - canvas_size) / 2;

    /* Background */
    SDL_Rect bg = {canvas_x, canvas_y, canvas_size, canvas_size};
    SDL_SetRenderDrawColor(app->renderer, 10, 10, 26, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    /* Grid */
    for (int v = -SPR_HALF; v <= SPR_HALF; v++) {
        int gx, gy, gx2, gy2;
        spr_vx_to_px(v, -SPR_HALF, canvas_x, canvas_y, canvas_size, canvas_size, &gx, &gy);
        spr_vx_to_px(v, SPR_HALF, canvas_x, canvas_y, canvas_size, canvas_size, &gx2, &gy2);
        SDL_Color gc = (v % 5 == 0) ? (SDL_Color){42,42,62,255} : (SDL_Color){26,26,46,255};
        SDL_SetRenderDrawColor(app->renderer, gc.r, gc.g, gc.b, 255);
        SDL_RenderDrawLine(app->renderer, gx, gy2, gx, gy);

        spr_vx_to_px(-SPR_HALF, v, canvas_x, canvas_y, canvas_size, canvas_size, &gx, &gy);
        spr_vx_to_px(SPR_HALF, v, canvas_x, canvas_y, canvas_size, canvas_size, &gx2, &gy2);
        SDL_RenderDrawLine(app->renderer, gx, gy, gx2, gy);
    }

    /* Axes */
    int ax1, ay1, ax2, ay2;
    spr_vx_to_px(0, -SPR_HALF, canvas_x, canvas_y, canvas_size, canvas_size, &ax1, &ay1);
    spr_vx_to_px(0, SPR_HALF, canvas_x, canvas_y, canvas_size, canvas_size, &ax2, &ay2);
    SDL_SetRenderDrawColor(app->renderer, 50, 50, 85, 255);
    SDL_RenderDrawLine(app->renderer, ax1, ay1, ax2, ay2);
    spr_vx_to_px(-SPR_HALF, 0, canvas_x, canvas_y, canvas_size, canvas_size, &ax1, &ay1);
    spr_vx_to_px(SPR_HALF, 0, canvas_x, canvas_y, canvas_size, canvas_size, &ax2, &ay2);
    SDL_RenderDrawLine(app->renderer, ax1, ay1, ax2, ay2);

    if (frame && frame->count > 0) {
        /* Draw lines between consecutive points */
        SDL_SetRenderDrawColor(app->renderer, 220, 220, 255, 255);
        for (int i = 1; i < frame->count; i++) {
            int px1, py1, px2, py2;
            spr_vx_to_px(frame->points[i-1].x, frame->points[i-1].y,
                         canvas_x, canvas_y, canvas_size, canvas_size, &px1, &py1);
            spr_vx_to_px(frame->points[i].x, frame->points[i].y,
                         canvas_x, canvas_y, canvas_size, canvas_size, &px2, &py2);
            SDL_RenderDrawLine(app->renderer, px1, py1, px2, py2);
        }

        /* Draw point handles */
        for (int i = 0; i < frame->count; i++) {
            int hx, hy;
            spr_vx_to_px(frame->points[i].x, frame->points[i].y,
                         canvas_x, canvas_y, canvas_size, canvas_size, &hx, &hy);
            SDL_Color hc;
            if (sel_pt == i)       hc = (SDL_Color){255, 70, 70, 255};
            else if (i == 0)       hc = (SDL_Color){255, 255, 0, 255};
            else                   hc = (SDL_Color){130, 170, 255, 255};
            SDL_SetRenderDrawColor(app->renderer, hc.r, hc.g, hc.b, 255);
            SDL_Rect hr = {hx - 3, hy - 3, 6, 6};
            SDL_RenderFillRect(app->renderer, &hr);

            /* Point index */
            char idx[4];
            snprintf(idx, sizeof(idx), "%d", i);
            ui_text_small(hx + 6, hy - 8, idx);
        }

        /* Bounding box */
        int minx = frame->points[0].x, maxx = minx;
        int miny = frame->points[0].y, maxy = miny;
        for (int i = 1; i < frame->count; i++) {
            if (frame->points[i].x < minx) minx = frame->points[i].x;
            if (frame->points[i].x > maxx) maxx = frame->points[i].x;
            if (frame->points[i].y < miny) miny = frame->points[i].y;
            if (frame->points[i].y > maxy) maxy = frame->points[i].y;
        }
        int hw = abs(minx) > abs(maxx) ? abs(minx) : abs(maxx);
        int hh = abs(miny) > abs(maxy) ? abs(miny) : abs(maxy);
        int bx1, by1, bx2, by2;
        spr_vx_to_px(-hw, -hh, canvas_x, canvas_y, canvas_size, canvas_size, &bx1, &by1);
        spr_vx_to_px(hw, hh, canvas_x, canvas_y, canvas_size, canvas_size, &bx2, &by2);
        SDL_SetRenderDrawColor(app->renderer, 68, 85, 100, 255);
        SDL_Rect bbr = {bx2, by2, bx1 - bx2, by1 - by2};
        SDL_RenderDrawRect(app->renderer, &bbr);

        /* BBox + VLC info */
        char bbox_info[64];
        snprintf(bbox_info, sizeof(bbox_info), "BBox: HW=%d HH=%d  Points: %d", hw, hh, frame->count);
        ui_text_mono_color(canvas_x + 4, canvas_y + 4, bbox_info, (SDL_Color){100,130,130,255});

        /* VLC data */
        if (frame->count >= 2) {
            char vlc[256];
            int n = snprintf(vlc, sizeof(vlc), "VLC: %d", frame->count - 2);
            for (int i = 1; i < frame->count && n < 240; i++) {
                int dy = frame->points[i].y - frame->points[i-1].y;
                int dx = frame->points[i].x - frame->points[i-1].x;
                n += snprintf(vlc + n, sizeof(vlc) - n, ", %d, %d", clamp_i(dy, -128, 127), clamp_i(dx, -128, 127));
            }
            ui_text_mono_color(canvas_x + 4, canvas_y + canvas_size - 16, vlc, (SDL_Color){100,100,130,255});
        }
    }

    /* Mouse interaction */
    if (ui_mouse_in_rect(canvas_x, canvas_y, canvas_size, canvas_size)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int vx, vy;
        spr_px_to_vx(mx, my, canvas_x, canvas_y, canvas_size, canvas_size, &vx, &vy);

        char coord[32];
        snprintf(coord, sizeof(coord), "(%d, %d)", vx, vy);
        ui_tooltip(coord);

        if (spr_draw_mode) {
            /* Draw mode: left click adds point */
            if (ui_mouse_clicked() && frame && frame->count < MAX_POINTS) {
                frame->points[frame->count++] = (Point){vx, vy};
                app->modified = true;
            }
            /* Right click: undo last point */
            if (ui_mouse_right_clicked() && frame && frame->count > 0) {
                frame->count--;
                app->modified = true;
            }
        } else {
            /* Select mode */
            if (ui_mouse_clicked()) {
                sel_pt = -1;
                if (frame) {
                    for (int i = 0; i < frame->count; i++) {
                        int hx, hy;
                        spr_vx_to_px(frame->points[i].x, frame->points[i].y,
                                     canvas_x, canvas_y, canvas_size, canvas_size, &hx, &hy);
                        if (abs(mx - hx) < 8 && abs(my - hy) < 8) {
                            sel_pt = i; break;
                        }
                    }
                }
            }
            /* Drag selected point */
            if (sel_pt >= 0 && frame && sel_pt < frame->count && ui_mouse_down()) {
                frame->points[sel_pt] = (Point){vx, vy};
                app->modified = true;
            }
        }
    }

    /* Delete selected point */
    if (!spr_draw_mode && sel_pt >= 0 && frame && sel_pt < frame->count &&
        (ui_key_pressed(SDLK_DELETE) || ui_key_pressed(SDLK_BACKSPACE))) {
        for (int i = sel_pt; i < frame->count - 1; i++)
            frame->points[i] = frame->points[i + 1];
        frame->count--;
        sel_pt = -1;
        app->modified = true;
    }

    /* Keyboard shortcuts */
    if (ui_key_pressed(SDLK_d)) spr_draw_mode = true;
    if (ui_key_pressed(SDLK_s)) spr_draw_mode = false;

    ui_panel_end();
}
