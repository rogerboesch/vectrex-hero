/*
 * app.c — QL Studio 2 core: init, tab bar, main layout, keyboard shortcuts
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

/* ── Tab bar ──────────────────────────────────────────────── */

static void draw_tab_bar(App *app) {
    SDL_Rect bar = {0, 0, app->win_w, STYLE_TAB_BAR_H};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.panel_title.r, ui_theme.panel_title.g,
                           ui_theme.panel_title.b, ui_theme.panel_title.a);
    SDL_RenderFillRect(app->renderer, &bar);

    const char *tabs[] = {"Sprites", "Images", "Emulator"};
    ViewMode modes[] = {VIEW_SPRITES, VIEW_IMAGES, VIEW_EMULATOR};
    int tab_w = STYLE_TAB_W;
    int pad = 4;

    for (int i = 0; i < 3; i++) {
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

    /* Tab bar at top */
    draw_tab_bar(app);

    /* Content below tab bar, above console */
    int top = STYLE_TAB_BAR_H;
    int con_y = app->win_h - STYLE_CONSOLE_H;
    int ch = con_y - top;

    /* Left panel */
    draw_left_panel(app, 0, top, STYLE_LEFT_PANEL_W, ch);

    /* Console at bottom, full width */
    draw_console(app, 0, con_y, app->win_w, STYLE_CONSOLE_H);

    /* Center + right panels depend on view */
    int cx = STYLE_LEFT_PANEL_W;
    int cw = app->win_w - STYLE_LEFT_PANEL_W - STYLE_RIGHT_PANEL_W;
    int rw = STYLE_RIGHT_PANEL_W;
    int rx = app->win_w - rw;

    if (app->view == VIEW_SPRITES) {
        draw_sprite_canvas(app, cx, top, cw, ch);
        draw_tools_panel(app, rx, top, rw, ch);
    } else if (app->view == VIEW_IMAGES) {
        draw_image_canvas(app, cx, top, cw, ch);
        draw_image_tools(app, rx, top, rw, ch);
    } else if (app->view == VIEW_EMULATOR) {
        draw_emulator(app, cx, top, cw, ch);
        /* Right panel: CPU top half, Memory bottom half */
        draw_cpu_state(app, rx, top, rw, ch / 2);
        draw_memory(app, rx, top + ch / 2, rw, ch - ch / 2);
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
