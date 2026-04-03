/*
 * app.c — GBC Studio core
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

void app_log(App *app, const char *fmt, ...) { va_list a; va_start(a, fmt); log_append(app, "", fmt, a); va_end(a); }
void app_log_dbg(App *app, const char *fmt, ...) { va_list a; va_start(a, fmt); log_append(app, "- ", fmt, a); va_end(a); }
void app_log_info(App *app, const char *fmt, ...) { va_list a; va_start(a, fmt); log_append(app, "> ", fmt, a); va_end(a); }
void app_log_warn(App *app, const char *fmt, ...) { va_list a; va_start(a, fmt); log_append(app, "* ", fmt, a); va_end(a); }
void app_log_err(App *app, const char *fmt, ...) { va_list a; va_start(a, fmt); log_append(app, "! ", fmt, a); va_end(a); }

/* ── Init ─────────────────────────────────────────────────── */

static void load_default_sprites(App *app);

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer) {
    memset(app, 0, sizeof(*app));
    app->window = window;
    app->renderer = renderer;
    app->view = VIEW_TILES;
    app->cur_color = 3;

    /* Default palettes matching tiles.c */
    /* Sprite palette 0 — player green */
    app->spr_pals[0] = (GBCPalette){{{0,0,0},{0,12,0},{0,24,4},{8,31,8}}};
    app->spr_pals[1] = (GBCPalette){{{0,0,0},{20,0,0},{31,8,4},{31,20,16}}};
    app->spr_pals[2] = (GBCPalette){{{0,0,0},{16,0,16},{26,6,26},{31,18,31}}};
    app->spr_pals[3] = (GBCPalette){{{0,0,0},{0,10,16},{0,22,28},{12,31,31}}};
    app->spr_pals[4] = (GBCPalette){{{0,0,0},{16,16,0},{28,28,4},{31,31,20}}};

    app->bg_pals[0] = (GBCPalette){{{0,0,0},{8,6,4},{16,12,8},{24,20,16}}};
    app->bg_pals[1] = (GBCPalette){{{0,0,0},{16,12,0},{24,20,4},{31,28,8}}};
    app->bg_pals[2] = (GBCPalette){{{0,0,0},{20,4,0},{31,10,0},{31,24,4}}};
    app->bg_pals[3] = (GBCPalette){{{0,0,0},{0,10,0},{0,22,0},{28,31,28}}};
    app->bg_pals[4] = (GBCPalette){{{0,0,0},{0,0,0},{2,2,4},{4,4,6}}};
    app->bg_pals[5] = (GBCPalette){{{0,0,0},{20,0,0},{31,4,4},{31,31,31}}};

    app->room_tool = TOOL_SELECT;
    project_init(&app->level_project);
    load_default_sprites(app);
    app_log_info(app, "GBC Studio ready");
}

void app_cleanup(App *app) { (void)app; }

/* Coordinate helpers for room editor */
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

/* ── Load hardcoded sprites from tiles.c data ─────────────── */

static void add_tile(App *app, const char *name, const uint8_t *data, int pal) {
    if (app->tile_count >= MAX_TILES) return;
    GBCTile *t = &app->tiles[app->tile_count];
    strncpy(t->name, name, sizeof(t->name) - 1);
    memcpy(t->data, data, TILE_BYTES);
    t->palette = pal;
    app->tile_count++;
}

/* Embed the sprite data directly so we have something to show */
static void load_default_sprites(App *app) {
    static const uint8_t spr_player_r[] = {
        0x00,0x00, 0x38,0x38, 0x44,0x7C, 0x00,0x6C, 0x44,0x7C, 0x00,0x10,
        0x7C,0x7C, 0x00,0x7C, 0x00,0x7C, 0x7C,0x00, 0x00,0x30, 0x00,0x30,
        0x00,0x18, 0x00,0x18, 0x00,0x24, 0x66,0x00,
    };
    static const uint8_t spr_player_walk[] = {
        0x00,0x00, 0x38,0x38, 0x44,0x7C, 0x00,0x6C, 0x44,0x7C, 0x00,0x10,
        0x7C,0x7C, 0x00,0x7C, 0x00,0x7C, 0x7C,0x00, 0x00,0x30, 0x00,0x28,
        0x00,0x24, 0x00,0x12, 0x00,0x22, 0x66,0x00,
    };
    static const uint8_t spr_player_fly[] = {
        0x00,0x00, 0x38,0x38, 0x44,0x7C, 0x00,0x6C, 0x44,0x7C, 0x00,0x10,
        0x7C,0x7C, 0x00,0x7C, 0x00,0x7C, 0x7C,0x00, 0x00,0x18, 0x00,0x18,
        0x00,0x24, 0x00,0x24, 0x00,0x42, 0x00,0x42,
    };
    static const uint8_t spr_bat0[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x42,0x42, 0x24,0x24,
        0x3C,0x3C, 0x18,0x18, 0x18,0x18, 0x00,0x18, 0x00,0x00, 0x00,0x00,
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    };
    static const uint8_t spr_spider[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x42,0x42,
        0x24,0x24, 0x18,0x18, 0x3C,0x3C, 0x42,0x42, 0x00,0x00, 0x00,0x00,
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    };
    static const uint8_t spr_snake_r[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
        0x70,0x70, 0x18,0x18, 0x0E,0x0E, 0x18,0x18, 0x60,0x60, 0x00,0x00,
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    };
    static const uint8_t spr_miner[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x38, 0x00,0x7C, 0x00,0x38,
        0x00,0x10, 0x00,0x7C, 0x00,0x7C, 0x00,0x7C, 0x00,0x30, 0x00,0x30,
        0x00,0x18, 0x00,0x18, 0x00,0x24, 0x00,0x24,
    };

    add_tile(app, "player_r",    spr_player_r,    0);
    add_tile(app, "player_walk", spr_player_walk, 0);
    add_tile(app, "player_fly",  spr_player_fly,  0);
    add_tile(app, "bat0",        spr_bat0,        1);
    add_tile(app, "spider",      spr_spider,      2);
    add_tile(app, "snake_r",     spr_snake_r,     1);
    add_tile(app, "miner",       spr_miner,       3);
}

/* ── Tab bar ──────────────────────────────────────────────── */

static void draw_tab_bar(App *app) {
    SDL_Rect bar = {0, 0, app->win_w, STYLE_TAB_BAR_H};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.panel_title.r, ui_theme.panel_title.g,
                           ui_theme.panel_title.b, 255);
    SDL_RenderFillRect(app->renderer, &bar);

    const char *tabs[] = {"Tiles", "Palettes", "Rooms", "Rows", "Emulator"};
    ViewMode modes[] = {VIEW_TILES, VIEW_PALETTES, VIEW_ROOMS, VIEW_ROWS, VIEW_EMULATOR};
    int tab_w = STYLE_TAB_W, pad = 4;

    for (int i = 0; i < 5; i++) {
        int tx = pad + i * (tab_w + pad), ty = 3, th = STYLE_TAB_BAR_H - 6;
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

    SDL_SetRenderDrawColor(app->renderer, ui_theme.bg.r, ui_theme.bg.g, ui_theme.bg.b, 255);
    SDL_RenderClear(app->renderer);

    draw_tab_bar(app);

    int top = STYLE_TAB_BAR_H;
    int con_y = app->win_h - STYLE_CONSOLE_H;
    int ch = con_y - top;
    int lw = STYLE_LEFT_PANEL_W, rw = STYLE_RIGHT_PANEL_W;
    int cx = lw, cw = app->win_w - lw - rw;

    draw_console(app, 0, con_y, app->win_w, STYLE_CONSOLE_H);

    switch (app->view) {
    case VIEW_TILES:
        draw_tile_list(app, 0, top, lw, ch);
        draw_tile_editor(app, cx, top, cw, ch);
        draw_tile_tools(app, app->win_w - rw, top, rw, ch);
        break;
    case VIEW_PALETTES:
        draw_palette_editor(app, 0, top, app->win_w, ch);
        break;
    case VIEW_ROOMS:
        draw_level_list(app, 0, top, lw, ch);
        draw_room_editor(app, cx, top, cw, ch);
        draw_room_tools(app, app->win_w - rw, top, rw, ch);
        break;
    case VIEW_ROWS:
        draw_row_tools(app, 0, top, lw, ch);
        draw_row_editor(app, cx, top, cw + rw, ch);
        break;
    case VIEW_EMULATOR:
        draw_gbp(app, 0, top, lw, ch / 2);
        draw_gmem(app, 0, top + ch / 2, lw, ch - ch / 2);
        draw_gbc_emulator(app, cx, top, cw, ch);
        draw_gcpu(app, app->win_w - rw, top, rw, ch / 2);
        draw_gdisasm(app, app->win_w - rw, top + ch / 2, rw, ch - ch / 2);
        break;
    }
}
