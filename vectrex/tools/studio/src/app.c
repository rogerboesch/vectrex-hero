/*
 * app.c — Vectrex Studio core: init, tab bar, main layout
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

/* ── Tab bar ──────────────────────────────────────────────── */

static void draw_tab_bar(App *app) {
    SDL_Rect bar = {0, 0, app->win_w, STYLE_TAB_BAR_H};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.panel_title.r, ui_theme.panel_title.g,
                           ui_theme.panel_title.b, ui_theme.panel_title.a);
    SDL_RenderFillRect(app->renderer, &bar);

    const char *tabs[] = {"Rooms", "Rows", "Sprites", "Levels", "Emulator"};
    ViewMode modes[] = {VIEW_ROOM_EDITOR, VIEW_ROW_EDITOR, VIEW_SPRITE_EDITOR, VIEW_LEVEL_VIEWER, VIEW_EMULATOR};
    int tab_w = STYLE_TAB_W;
    int pad = 4;

    for (int i = 0; i < 5; i++) {
        int tx = pad + i * (tab_w + pad);
        int ty = 3;
        int th = STYLE_TAB_BAR_H - 6;
        bool active = (app->view == modes[i]);
        bool hover = ui_mouse_in_rect(tx, ty, tab_w, th);

        SDL_Color bg = active ? ui_theme.panel_bg : hover ? ui_theme.btn_hover : ui_theme.panel_title;
        SDL_Rect r = {tx, ty, tab_w, th};
        SDL_SetRenderDrawColor(app->renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(app->renderer, &r);

        int tw = ui_text_width(tabs[i]);
        ui_text_color(tx + (tab_w - tw) / 2, ty + (th - ui_line_height()) / 2,
                      tabs[i], active ? ui_theme.text : ui_theme.text_dim);

        if (hover && ui_mouse_clicked()) app->view = modes[i];

        if (active) {
            SDL_Rect hl = {tx, STYLE_TAB_BAR_H - 3, tab_w, 3};
            SDL_SetRenderDrawColor(app->renderer, ui_theme.tab_active.r, ui_theme.tab_active.g, ui_theme.tab_active.b, 255);
            SDL_RenderFillRect(app->renderer, &hl);
        }
    }
}

/* ── Main draw ────────────────────────────────────────────── */

void app_draw(App *app) {
    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
    SDL_RenderSetLogicalSize(app->renderer, app->win_w, app->win_h);

    SDL_SetRenderDrawColor(app->renderer, ui_theme.bg.r, ui_theme.bg.g,
                           ui_theme.bg.b, ui_theme.bg.a);
    SDL_RenderClear(app->renderer);

    draw_tab_bar(app);

    int top = STYLE_TAB_BAR_H;
    int con_y = app->win_h - STYLE_CONSOLE_H;
    int ch = con_y - top;
    int lw = STYLE_LEFT_PANEL_W;
    int rw = STYLE_RIGHT_PANEL_W;
    int cx = lw;
    int cw = app->win_w - lw - rw;

    /* Left panel — context-dependent */
    if (app->view == VIEW_ROW_EDITOR)
        draw_row_tools(app, 0, top, lw, ch);
    else if (app->view == VIEW_SPRITE_EDITOR)
        draw_sprite_tools(app, 0, top, lw, ch);
    else if (app->view == VIEW_EMULATOR) {
        /* Breakpoints top half, memory bottom half */
        draw_vmem(app, 0, top, lw, ch / 2);
        /* placeholder for breakpoints panel — reuse memory for now */
        ui_panel_begin("Breakpoints", 0, top + ch / 2, lw, ch - ch / 2);
        ui_text_color(4, top + ch / 2 + 30, "Coming soon", ui_theme.text_dim);
        ui_panel_end();
    } else
        draw_level_list(app, 0, top, lw, ch);

    /* Console */
    draw_console(app, 0, con_y, app->win_w, STYLE_CONSOLE_H);

    /* Center + right */
    switch (app->view) {
    case VIEW_ROOM_EDITOR:
        draw_room_editor(app, cx, top, cw, ch);
        draw_room_tools(app, app->win_w - rw, top, rw, ch);
        break;
    case VIEW_ROW_EDITOR:
        draw_row_editor(app, cx, top, cw + rw, ch);
        break;
    case VIEW_SPRITE_EDITOR:
        draw_sprite_editor(app, cx, top, cw + rw, ch);
        break;
    case VIEW_LEVEL_VIEWER:
        draw_level_viewer(app, cx, top, cw + rw, ch);
        break;
    case VIEW_EMULATOR:
        draw_emu_panel(app, cx, top, cw, ch);
        draw_vcpu(app, app->win_w - rw, top, rw, ch / 2);
        draw_vdisasm(app, app->win_w - rw, top + ch / 2, rw, ch - ch / 2);
        break;
    }
}
