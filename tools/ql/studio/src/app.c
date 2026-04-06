/*
 * app.c — QL Studio 2 core (Activity bar + Status bar layout)
 */

#include "app.h"
#include "ui.h"
#include "style.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── Helpers ──────────────────────────────────────────────── */

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
    va_list args; va_start(args, fmt);
    log_append(app, "", fmt, args);
    va_end(args);
}

void app_log_dbg(App *app, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    log_append(app, "- ", fmt, args);
    va_end(args);
}

void app_log_info(App *app, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    log_append(app, "> ", fmt, args);
    va_end(args);
}

void app_log_warn(App *app, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    log_append(app, "* ", fmt, args);
    va_end(args);
}

void app_log_err(App *app, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    log_append(app, "! ", fmt, args);
    va_end(args);
}

/* ── Init / cleanup ───────────────────────────────────────── */

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer) {
    memset(app, 0, sizeof(*app));
    app->window = window;
    app->renderer = renderer;
    app->view = VIEW_SPRITES;
    app->current_color = 7;
    app->image_zoom = 2;
    app->speed_idx = 1;
    app->bp_enabled = true;

    /* Default sprite */
    sprite_init(&app->sprites[0], "sprite_0", 10, 20);
    app->sprite_count = 1;

    app_log_info(app, "QL Studio 2 ready");
}

void app_cleanup(App *app) {
    if (app->canvas_tex) SDL_DestroyTexture(app->canvas_tex);
    if (app->preview_tex) SDL_DestroyTexture(app->preview_tex);
    if (app->image_tex) SDL_DestroyTexture(app->image_tex);
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

    /* Icons */
    int icon_h = ab_w;
    struct { uint16_t icon; ViewMode mode; } items[] = {
        { ICON_SYMBOL_COLOR, VIEW_SPRITES },
        { ICON_FILE_MEDIA,   VIEW_IMAGES },
        { ICON_VM_RUNNING,   VIEW_EMULATOR },
    };
    for (int i = 0; i < 3; i++) {
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

    /* Sprite/image count */
    if (app->view == VIEW_SPRITES) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Sprites: %d", app->sprite_count);
        x += ui_status_item(x, r.y, buf) + 16;
    } else if (app->view == VIEW_IMAGES) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Images: %d", app->image_count);
        x += ui_status_item(x, r.y, buf) + 16;
    }

    /* Modified */
    if (app->modified) {
        x += ui_status_item(x, r.y, "Modified") + 16;
    }

    /* Right-aligned: view name */
    const char *views[] = {"Sprites", "Images", "Emulator"};
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

    /* Context */
    static char ctx_buf[64];
    switch (app->view) {
    case VIEW_SPRITES:
        if (app->current_sprite < app->sprite_count) {
            snprintf(ctx_buf, sizeof(ctx_buf), "%s", app->sprites[app->current_sprite].name);
            segs[count++] = ctx_buf;
        }
        segs[count++] = "Sprite Editor";
        break;
    case VIEW_IMAGES:
        if (app->current_image < app->image_count) {
            snprintf(ctx_buf, sizeof(ctx_buf), "%s", app->images[app->current_image].name);
            segs[count++] = ctx_buf;
        }
        segs[count++] = "Image Editor";
        break;
    case VIEW_EMULATOR:
        segs[count++] = "Emulator";
        break;
    }

    ui_breadcrumb(br.x, br.y, segs, count);
}

/* ── Tools panel (combines palette + properties + preview) ── */

static void draw_tools_panel(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int y = c.y;
    draw_palette(app, c.x - 4, y, c.w + 8, &y);
    draw_properties(app, c.x - 4, y, c.w + 8, &y);
    draw_preview(app, c.x - 4, y, c.w + 8, &y);

    ui_panel_end();
}

/* ── Left panel ───────────────────────────────────────────── */

static void draw_left_panel(App *app, int px, int py, int pw, int ph) {
    if (app->view == VIEW_SPRITES) {
        draw_sprite_list(app, px, py, pw, ph);
    } else if (app->view == VIEW_IMAGES) {
        draw_image_list(app, px, py, pw, ph);
    } else {
        draw_breakpoints(app, px, py, pw, ph);
    }
}

/* ── Main draw ────────────────────────────────────────────── */

void app_draw(App *app) {
    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
    SDL_RenderSetLogicalSize(app->renderer, app->win_w, app->win_h);

    /* Background */
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
    int rx = app->win_w - rw;

    /* Left panel */
    draw_left_panel(app, ab, top, lw, panel_h);

    /* Console (above status bar) */
    draw_console(app, ab, con_y, app->win_w - ab, con_h);

    /* Breadcrumb bar (top of center panel) */
    draw_breadcrumb(app, cx, top, cw);

    int center_y = top + bc;
    int center_h = panel_h - bc;

    /* Center + right panels depend on view */
    if (app->view == VIEW_SPRITES) {
        draw_sprite_canvas(app, cx, center_y, cw, center_h);
        draw_tools_panel(app, rx, top, rw, panel_h);
    } else if (app->view == VIEW_IMAGES) {
        draw_image_canvas(app, cx, center_y, cw, center_h);
        draw_image_tools(app, rx, top, rw, panel_h);
    } else if (app->view == VIEW_EMULATOR) {
        draw_emulator(app, cx, center_y, cw, center_h);
        /* Right panel: CPU / Disassembly / Memory (thirds) */
        int rh3 = panel_h / 3;
        draw_cpu_state(app, rx, top, rw, rh3);
        draw_disasm(app, rx, top + rh3, rw, rh3);
        draw_memory(app, rx, top + rh3 * 2, rw, panel_h - rh3 * 2);
    }

    /* Keyboard shortcuts */
    if (ui_key_mod_cmd() && !app->name_focus) {
        if (app->current_sprite < app->sprite_count) {
            Sprite *spr = &app->sprites[app->current_sprite];
            if (ui_key_pressed(SDLK_h)) sprite_flip_h(spr);
            if (ui_key_pressed(SDLK_j)) sprite_flip_v(spr);
            if (ui_key_pressed(SDLK_UP)) sprite_move_up(spr);
            if (ui_key_pressed(SDLK_DOWN)) sprite_move_down(spr);
            if (ui_key_pressed(SDLK_LEFT)) sprite_move_left(spr);
            if (ui_key_pressed(SDLK_RIGHT)) sprite_move_right(spr);
            if (ui_key_pressed(SDLK_DELETE) || ui_key_pressed(SDLK_BACKSPACE))
                sprite_clear(spr);
            if (ui_key_pressed(SDLK_c)) {
                app->clipboard = *spr;
                app->has_clipboard = 1;
            }
            if (ui_key_pressed(SDLK_v) && app->has_clipboard) {
                memcpy(spr->pixels, app->clipboard.pixels, sizeof(spr->pixels));
                sprite_resize(spr, app->clipboard.width, app->clipboard.height);
            }
        }
    }
}
