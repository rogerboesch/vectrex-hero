/*
 * app.c — Vectrex Studio core (Activity bar + Status bar layout)
 */

#include "app.h"
#include "ui.h"
#include "style.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── Logging ──────────────────────────────────────────────── */

static void log_append(App *app, const char *prefix, const char *fmt, va_list args) {
    if (app->console_count >= 64) {
        memmove(app->console_lines[0], app->console_lines[1], 63 * 256);
        app->console_count = 63;
    }
    char *line = app->console_lines[app->console_count];
    int off = snprintf(line, 256, "%s", prefix);
    vsnprintf(line + off, 256 - off, fmt, args);
    app->console_count++;
}

void app_log(App *app, const char *fmt, ...) {
    va_list a; va_start(a, fmt); log_append(app, "", fmt, a); va_end(a);
}
void app_log_dbg(App *app, const char *fmt, ...) {
    va_list a; va_start(a, fmt); log_append(app, "- ", fmt, a); va_end(a);
}
void app_log_info(App *app, const char *fmt, ...) {
    va_list a; va_start(a, fmt); log_append(app, "> ", fmt, a); va_end(a);
}
void app_log_warn(App *app, const char *fmt, ...) {
    va_list a; va_start(a, fmt); log_append(app, "* ", fmt, a); va_end(a);
}
void app_log_err(App *app, const char *fmt, ...) {
    va_list a; va_start(a, fmt); log_append(app, "! ", fmt, a); va_end(a);
}

/* ── Init / cleanup ───────────────────────────────────────── */

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer) {
    memset(app, 0, sizeof(*app));
    app->window = window;
    app->renderer = renderer;
    app->view = VIEW_ROOM_EDITOR;
    app->drag_idx = -1;
    app->room_tool = TOOL_SELECT;
    project_init(&app->project);
    app_log_info(app, "Vectrex Studio ready");
}

void app_cleanup(App *app) {
    (void)app;
}

/* ── Coordinate helpers ───────────────────────────────────── */

void room_vx_to_px(int vx, int vy, int cx, int cy, int cw, int ch, int *px, int *py) {
    *px = cx + (int)((float)(vx - ROOM_LEFT) / 250.0f * cw);
    *py = cy + (int)((float)(ROOM_TOP - vy) / 100.0f * ch);
}

void room_px_to_vx(int px, int py, int cx, int cy, int cw, int ch, int *vx, int *vy) {
    *vx = ROOM_LEFT + (int)((float)(px - cx) / cw * 250.0f);
    *vy = ROOM_TOP - (int)((float)(py - cy) / ch * 100.0f);
    *vx = clamp_i(*vx, ROOM_LEFT, ROOM_RIGHT);
    *vy = clamp_i(*vy, ROOM_BOTTOM, ROOM_TOP);
}

/* ── Activity bar ─────────────────────────────────────────── */

static void draw_activity_bar(App *app) {
    int ab_w = STYLE_ACTIVITY_BAR_W;
    int ab_h = app->win_h - STYLE_STATUS_BAR_H;

    /* Background */
    SDL_Rect bar = {0, 0, ab_w, ab_h};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.activity_bg.r, ui_theme.activity_bg.g,
                           ui_theme.activity_bg.b, 255);
    SDL_RenderFillRect(app->renderer, &bar);

    /* Right border */
    SDL_SetRenderDrawColor(app->renderer, ui_theme.border.r, ui_theme.border.g, ui_theme.border.b, 255);
    SDL_RenderDrawLine(app->renderer, ab_w - 1, 0, ab_w - 1, ab_h);

    /* Icons — vertically stacked, each 48x48 */
    int icon_h = ab_w;
    struct { uint16_t icon; ViewMode mode; } items[] = {
        { ICON_HOME,         VIEW_ROOM_EDITOR },
        { ICON_EDIT,         VIEW_ROW_EDITOR },
        { ICON_SYMBOL_COLOR, VIEW_SPRITE_EDITOR },
        { ICON_EYE,          VIEW_LEVEL_VIEWER },
        { ICON_VM_RUNNING,   VIEW_EMULATOR },
    };
    for (int i = 0; i < 5; i++) {
        int iy = i * icon_h;
        if (ui_activity_icon(0, iy, ab_w, icon_h, items[i].icon, app->view == items[i].mode))
            app->view = items[i].mode;
    }
}

/* ── Status bar ───────────────────────────────────────────── */

static void draw_status_bar(App *app) {
    int sb_h = STYLE_STATUS_BAR_H;
    int sb_y = app->win_h - sb_h;
    SDL_Rect r = ui_status_bar(0, sb_y, app->win_w, sb_h);

    int x = r.x;

    /* Project name */
    const char *proj = app->project_path[0] ? app->project_path : "No project";
    const char *slash = strrchr(proj, '/');
    const char *name = slash ? slash + 1 : proj;
    x += ui_status_item(x, r.y, name) + 16;

    /* Current level/room */
    if (app->cur_level >= 0 && app->cur_level < app->project.level_count) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Level %d/%d  Room %d",
                 app->cur_level + 1, app->project.level_count, app->cur_room + 1);
        x += ui_status_item(x, r.y, buf) + 16;
    }

    /* Modified */
    if (app->modified) {
        x += ui_status_item(x, r.y, "Modified") + 16;
    }

    /* Right-aligned: view name */
    const char *views[] = {"Rooms", "Rows", "Sprites", "Levels", "Emulator"};
    const char *vn = views[app->view];
    int vw = ui_text_width(vn);
    ui_status_item(r.x + r.w - vw, r.y, vn);
}

/* ── Breadcrumb bar ───────────────────────────────────────── */

static void draw_breadcrumb(App *app, int x, int y, int w) {
    int h = STYLE_BREADCRUMB_H;
    SDL_Rect br = ui_breadcrumb_bar(x, y, w, h);

    const char *segs[4];
    int count = 0;

    /* Project file */
    const char *proj = app->project_path[0] ? app->project_path : "untitled";
    const char *slash = strrchr(proj, '/');
    segs[count++] = slash ? slash + 1 : proj;

    /* Level */
    static char lvl_buf[32];
    if (app->cur_level >= 0 && app->cur_level < app->project.level_count) {
        snprintf(lvl_buf, sizeof(lvl_buf), "Level %d", app->cur_level + 1);
        segs[count++] = lvl_buf;
    }

    /* View-specific context */
    switch (app->view) {
    case VIEW_ROOM_EDITOR: {
        static char room_buf[32];
        snprintf(room_buf, sizeof(room_buf), "Room %d", app->cur_room + 1);
        segs[count++] = room_buf;
        break;
    }
    case VIEW_ROW_EDITOR:
        segs[count++] = "Row Editor";
        break;
    case VIEW_SPRITE_EDITOR:
        segs[count++] = "Sprite Editor";
        break;
    case VIEW_LEVEL_VIEWER:
        segs[count++] = "Level Viewer";
        break;
    case VIEW_EMULATOR:
        segs[count++] = "Emulator";
        break;
    }

    ui_breadcrumb(br.x, br.y, segs, count);
}

/* ── Main draw ────────────────────────────────────────────── */

void app_draw(App *app) {
    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
    SDL_RenderSetLogicalSize(app->renderer, app->win_w, app->win_h);

    SDL_SetRenderDrawColor(app->renderer, ui_theme.bg.r, ui_theme.bg.g,
                           ui_theme.bg.b, ui_theme.bg.a);
    SDL_RenderClear(app->renderer);

    /* Activity bar (left) */
    draw_activity_bar(app);

    /* Status bar (bottom) */
    draw_status_bar(app);

    /* Layout regions */
    int ab = STYLE_ACTIVITY_BAR_W;
    int sb = STYLE_STATUS_BAR_H;
    int bc = STYLE_BREADCRUMB_H;
    int con_h = STYLE_CONSOLE_H;
    int lw = STYLE_LEFT_PANEL_W;
    int rw = STYLE_RIGHT_PANEL_W;

    int top = 0;
    int total_h = app->win_h - sb;
    int con_y = total_h - con_h;
    int panel_h = con_y - top;

    int cx = ab + lw;
    int cw = app->win_w - ab - lw - rw;

    /* Console (above status bar) */
    draw_console(app, ab, con_y, app->win_w - ab, con_h);

    /* Breadcrumb bar (top of center panel) */
    draw_breadcrumb(app, cx, top, cw);

    int center_y = top + bc;
    int center_h = panel_h - bc;

    /* Left panel — context-dependent */
    if (app->view == VIEW_ROW_EDITOR)
        draw_row_tools(app, ab, top, lw, panel_h);
    else if (app->view == VIEW_SPRITE_EDITOR)
        draw_sprite_tools(app, ab, top, lw, panel_h);
    else if (app->view == VIEW_EMULATOR) {
        draw_vbp(app, ab, top, lw, panel_h / 2);
        draw_vmem(app, ab, top + panel_h / 2, lw, panel_h - panel_h / 2);
    } else
        draw_level_list(app, ab, top, lw, panel_h);

    /* Center + right */
    switch (app->view) {
    case VIEW_ROOM_EDITOR:
        draw_room_editor(app, cx, center_y, cw, center_h);
        draw_room_tools(app, app->win_w - rw, top, rw, panel_h);
        break;
    case VIEW_ROW_EDITOR:
        draw_row_editor(app, cx, center_y, cw + rw, center_h);
        break;
    case VIEW_SPRITE_EDITOR:
        draw_sprite_editor(app, cx, center_y, cw + rw, center_h);
        break;
    case VIEW_LEVEL_VIEWER:
        draw_level_viewer(app, cx, center_y, cw + rw, center_h);
        break;
    case VIEW_EMULATOR:
        draw_emu_panel(app, cx, center_y, cw, center_h);
        draw_vcpu(app, app->win_w - rw, top, rw, panel_h / 2);
        draw_vdisasm(app, app->win_w - rw, top + panel_h / 2, rw, panel_h - panel_h / 2);
        break;
    }
}
