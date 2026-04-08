/*
 * app.c — GBC Workbench core (Activity bar + Status bar layout)
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
    app->view = VIEW_LEVELS;
    app->cur_color = 3;
    app->sel_entity = -1;
    app->sel_type = SEL_TILE;

    tilemap_project_init(&app->tmap);
    load_default_sprites(app);
    app_log_info(app, "GBC Workbench ready");
}

void app_cleanup(App *app) { (void)app; }

/* ── Default sprites ──────────────────────────────────────── */

static void add_sprite(App *app, const char *name, const uint8_t *data, int pal) {
    if (app->sprite_count >= MAX_TILES) return;
    GBCTile *t = &app->sprites[app->sprite_count];
    strncpy(t->name, name, sizeof(t->name) - 1);
    memcpy(t->data, data, TILE_BYTES);
    t->palette = pal;
    app->sprite_count++;
}

static void load_default_sprites(App *app) {
    static const uint8_t spr_player_r[] = {
        0x00,0x00, 0x38,0x38, 0x44,0x7C, 0x00,0x6C, 0x44,0x7C, 0x00,0x10,
        0x7C,0x7C, 0x00,0x7C, 0x00,0x7C, 0x7C,0x00, 0x00,0x30, 0x00,0x30,
        0x00,0x18, 0x00,0x18, 0x00,0x24, 0x66,0x00,
    };
    static const uint8_t spr_bat0[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x42,0x42, 0x24,0x24,
        0x3C,0x3C, 0x18,0x18, 0x18,0x18, 0x00,0x18, 0x00,0x00, 0x00,0x00,
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    };
    static const uint8_t spr_miner[] = {
        0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x38, 0x00,0x7C, 0x00,0x38,
        0x00,0x10, 0x00,0x7C, 0x00,0x7C, 0x00,0x7C, 0x00,0x30, 0x00,0x30,
        0x00,0x18, 0x00,0x18, 0x00,0x24, 0x00,0x24,
    };
    add_sprite(app, "player_r", spr_player_r, 0);
    add_sprite(app, "bat0",     spr_bat0,     1);
    add_sprite(app, "miner",    spr_miner,    3);
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
        { ICON_LAYOUT,       VIEW_LEVELS },
        { ICON_SYMBOL_COLOR, VIEW_EDITOR },
        { ICON_PERSON,       VIEW_SPRITES },
        { ICON_FILE_MEDIA,   VIEW_SCREENS },
        { ICON_VM_RUNNING,   VIEW_EMULATOR },
    };
    for (int i = 0; i < 5; i++) {
        int iy = i * icon_h;
        if (ui_activity_icon(0, iy, ab_w, icon_h, items[i].icon, app->view == items[i].mode)) {
            app->view = items[i].mode;
            if (items[i].mode == VIEW_EDITOR)  app->sel_type = SEL_TILE;
            if (items[i].mode == VIEW_SCREENS) app->sel_type = SEL_TILE;
            if (items[i].mode == VIEW_SPRITES) app->sel_type = SEL_SPRITE;
        }
    }
}

/* ── Status bar ───────────────────────────────────────────── */

static void draw_status_bar(App *app) {
    int sb_h = STYLE_STATUS_BAR_H;
    int sb_y = app->win_h - sb_h;
    SDL_Rect r = ui_status_bar(0, sb_y, app->win_w, sb_h);

    int x = r.x;

    /* Project name or "No project" */
    const char *proj = app->project_path[0] ? app->project_path : "No project";
    /* Show just filename */
    const char *slash = strrchr(proj, '/');
    const char *name = slash ? slash + 1 : proj;
    x += ui_status_item(x, r.y, name) + 16;

    /* Current level */
    if (app->cur_level >= 0 && app->cur_level < app->tmap.level_count) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Level %d/%d", app->cur_level + 1, app->tmap.level_count);
        x += ui_status_item(x, r.y, buf) + 16;
    }

    /* Modified indicator */
    if (app->modified) {
        x += ui_status_item(x, r.y, "Modified") + 16;
    }

    /* Right-aligned: view name */
    const char *views[] = {"Levels", "Editor", "Sprites", "Emulator"};
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

    /* Level name */
    if (app->cur_level >= 0 && app->cur_level < app->tmap.level_count) {
        segs[count++] = app->tmap.levels[app->cur_level].name;
    }

    /* Layer */
    if (app->view == VIEW_LEVELS) {
        segs[count++] = (app->sel_type == SEL_SPRITE) ? "Sprite Layer" : "Tile Layer";
    } else if (app->view == VIEW_EDITOR) {
        segs[count++] = "Tile Editor";
    } else if (app->view == VIEW_SCREENS) {
        if (app->cur_screen < app->tmap.screen_count)
            segs[count++] = app->tmap.screens[app->cur_screen].name;
        segs[count++] = "Screen Editor";
    } else if (app->view == VIEW_SPRITES) {
        if (app->cur_sprite < app->sprite_count)
            segs[count++] = app->sprites[app->cur_sprite].name;
        segs[count++] = "Sprite Editor";
    }

    ui_breadcrumb(br.x, br.y, segs, count);
}

/* ── Main draw ────────────────────────────────────────────── */

void app_draw(App *app) {
    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
    SDL_RenderSetLogicalSize(app->renderer, app->win_w, app->win_h);

    SDL_SetRenderDrawColor(app->renderer, ui_theme.bg.r, ui_theme.bg.g, ui_theme.bg.b, 255);
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
    int lw = STYLE_LEFT_PANEL_W, rw = STYLE_RIGHT_PANEL_W;

    int top = 0;
    int total_h = app->win_h - sb;
    int con_y = total_h - con_h;
    int panel_h = con_y - top;

    int cx = ab + lw;
    int cw = app->win_w - ab - lw - rw;

    /* Console at bottom (above status bar) */
    draw_console(app, ab, con_y, app->win_w - ab, con_h);

    /* Breadcrumb bar (top of center panel) */
    draw_breadcrumb(app, cx, top, cw);

    /* Center content starts below breadcrumb */
    int center_y = top + bc;
    int center_h = panel_h - bc;

    switch (app->view) {
    case VIEW_LEVELS:
        draw_asset_list(app, ab, top, lw, panel_h);
        draw_level_editor(app, cx, center_y, cw, center_h);
        draw_level_tools(app, app->win_w - rw, top, rw, panel_h);
        break;
    case VIEW_EDITOR:
        draw_asset_list(app, ab, top, lw, panel_h);
        draw_pixel_editor(app, cx, center_y, cw, center_h);
        draw_editor_tools(app, app->win_w - rw, top, rw, panel_h);
        break;
    case VIEW_SPRITES:
        draw_sprite_list(app, ab, top, lw, panel_h);
        draw_pixel_editor(app, cx, center_y, cw, center_h);
        draw_editor_tools(app, app->win_w - rw, top, rw, panel_h);
        break;
    case VIEW_SCREENS:
        draw_asset_list(app, ab, top, lw, panel_h);
        draw_screen_editor(app, cx, center_y, cw, center_h);
        draw_editor_tools(app, app->win_w - rw, top, rw, panel_h);
        break;
    case VIEW_EMULATOR:
        draw_gbp(app, ab, top, lw, panel_h / 2);
        draw_gmem(app, ab, top + panel_h / 2, lw, panel_h - panel_h / 2);
        draw_gbc_emulator(app, cx, center_y, cw, center_h);
        draw_gcpu(app, app->win_w - rw, top, rw, panel_h / 2);
        draw_gdisasm(app, app->win_w - rw, top + panel_h / 2, rw, panel_h - panel_h / 2);
        break;
    }
}
