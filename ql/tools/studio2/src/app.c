/*
 * app.c — QL Studio 2 application
 *
 * Layout:
 *   [View Tabs] [Sprite List | Canvas                | Tools Panel]
 *               [            |                       |            ]
 */

#include "app.h"
#include "ui.h"
#include "style.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── Helpers ──────────────────────────────────────────────── */

void app_log(App *app, const char *fmt, ...) {
    if (app->console_count >= 64) {
        memmove(app->console_lines[0], app->console_lines[1], 63 * 256);
        app->console_count = 63;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->console_lines[app->console_count], 256, fmt, args);
    va_end(args);
    app->console_count++;
}

/* ── Init / cleanup ───────────────────────────────────────── */

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer) {
    memset(app, 0, sizeof(*app));
    app->window = window;
    app->renderer = renderer;
    app->view = VIEW_SPRITES;
    app->current_color = 7;

    /* Default sprite */
    sprite_init(&app->sprites[0], "sprite_0", 10, 20);
    app->sprite_count = 1;

    app_log(app, "QL Studio 2 ready");
}

void app_cleanup(App *app) {
    if (app->canvas_tex) SDL_DestroyTexture(app->canvas_tex);
    if (app->preview_tex) SDL_DestroyTexture(app->preview_tex);
}

/* ── Canvas texture update ────────────────────────────────── */

static void update_canvas_texture(App *app) {
    Sprite *spr = &app->sprites[app->current_sprite];

    /* Recreate texture if size changed */
    if (!app->canvas_tex || app->canvas_tex_w != spr->width || app->canvas_tex_h != spr->height) {
        if (app->canvas_tex) SDL_DestroyTexture(app->canvas_tex);
        app->canvas_tex = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             spr->width, spr->height);
        app->canvas_tex_w = spr->width;
        app->canvas_tex_h = spr->height;
    }

    /* Update pixels */
    uint32_t pixels[MAX_SPRITE_W * MAX_SPRITE_H];
    for (int y = 0; y < spr->height; y++) {
        for (int x = 0; x < spr->width; x++) {
            uint8_t c = spr->pixels[y][x];
            SDL_Color sc = QL_SDL_COLORS[c];
            pixels[y * spr->width + x] = (sc.a << 24) | (sc.b << 16) | (sc.g << 8) | sc.r;
        }
    }
    SDL_UpdateTexture(app->canvas_tex, NULL, pixels, spr->width * 4);
}

/* ── Tab bar — full width, fixed height at top ────────────── */

#define TAB_BAR_H STYLE_TAB_BAR_H

static void draw_tab_bar(App *app) {
    /* Background */
    SDL_Rect bar = {0, 0, app->win_w, TAB_BAR_H};
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
        int th = TAB_BAR_H - 6;
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

        /* Active indicator line */
        if (active) {
            SDL_Rect hl = {tx, TAB_BAR_H - 3, tab_w, 3};
            SDL_SetRenderDrawColor(app->renderer, ui_theme.tab_active.r, ui_theme.tab_active.g, ui_theme.tab_active.b, 255);
            SDL_RenderFillRect(app->renderer, &hl);
        }
    }
}

/* ── Left panel: context list ─────────────────────────────── */

static void draw_left_panel(App *app, int px, int py, int pw, int ph) {
    if (app->view == VIEW_SPRITES) {
        SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
        SDL_Rect c = ui_panel_content();
        int y = c.y;

        /* +/-/Dup in the title bar */
        int bbw = (tb.w - 8) / 3;
        if (ui_button(tb.x, tb.y, bbw, tb.h, "+")) {
            if (app->sprite_count < MAX_SPRITES) {
                char name[64];
                snprintf(name, sizeof(name), "sprite_%d", app->sprite_count);
                sprite_init(&app->sprites[app->sprite_count], name, 10, 20);
                app->current_sprite = app->sprite_count;
                app->sprite_count++;
            }
        }
        if (ui_button(tb.x + bbw + 4, tb.y, bbw, tb.h, "-")) {
            if (app->sprite_count > 1) {
                for (int i = app->current_sprite; i < app->sprite_count - 1; i++)
                    app->sprites[i] = app->sprites[i + 1];
                app->sprite_count--;
                if (app->current_sprite >= app->sprite_count)
                    app->current_sprite = app->sprite_count - 1;
            }
        }
        if (ui_button(tb.x + (bbw + 4) * 2, tb.y, bbw, tb.h, "Dup")) {
            if (app->sprite_count < MAX_SPRITES) {
                app->sprites[app->sprite_count] = app->sprites[app->current_sprite];
                /* Generate next available sprite_N name */
                char name[64];
                int n = app->sprite_count;
                for (;;) {
                    snprintf(name, sizeof(name), "sprite_%d", n);
                    int exists = 0;
                    for (int i = 0; i < app->sprite_count; i++) {
                        if (strcmp(app->sprites[i].name, name) == 0) { exists = 1; break; }
                    }
                    if (!exists) break;
                    n++;
                }
                strncpy(app->sprites[app->sprite_count].name, name, 63);
                app->current_sprite = app->sprite_count;
                app->sprite_count++;
            }
        }

        /* Sprite list */
        for (int i = 0; i < app->sprite_count; i++) {
            char label[80];
            snprintf(label, sizeof(label), "%s (%dx%d)",
                     app->sprites[i].name, app->sprites[i].width, app->sprites[i].height);
            if (ui_selectable(c.x, y, c.w, label, app->current_sprite == i)) {
                app->current_sprite = i;
                app->animate = false;
            }
            y += ui_line_height() + 2;
            if (y > c.y + c.h - ui_line_height()) break;
        }

        ui_panel_end();
    } else {
        const char *title = app->view == VIEW_IMAGES ? "Images" : "Emulator";
        ui_panel_begin(title, px, py, pw, ph);
        ui_panel_end();
    }
}

/* ── Sprite canvas ────────────────────────────────────────── */

/* ── Sprite canvas ────────────────────────────────────────── */

static void draw_sprite_canvas(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    if (app->current_sprite >= app->sprite_count) { ui_panel_end(); return; }
    Sprite *spr = &app->sprites[app->current_sprite];

    /* Toolbar buttons */
    int bx = tb.x;
    int bh = tb.h;
    int bw = 50;
    int gap = 4;

    if (ui_button(bx, tb.y, bw, bh, "L"))    { sprite_move_left(spr); }  bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "R"))    { sprite_move_right(spr); } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "U"))    { sprite_move_up(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "D"))    { sprite_move_down(spr); }  bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpH")) { sprite_flip_h(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpV")) { sprite_flip_v(spr); }    bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "Clear")){ sprite_clear(spr); }      bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "Copy")) {
        app->clipboard = *spr;
        app->has_clipboard = 1;
    }
    bx += bw + gap;
    if (app->has_clipboard) {
        if (ui_button(bx, tb.y, bw, bh, "Paste")) {
            memcpy(spr->pixels, app->clipboard.pixels, sizeof(spr->pixels));
            sprite_resize(spr, app->clipboard.width, app->clipboard.height);
        }
    }

    /* Calculate cell size to fit */
    float cell_x = (float)c.w / spr->width;
    float cell_y = (float)c.h / spr->height;
    float cell = cell_x < cell_y ? cell_x : cell_y;
    if (cell < 2) cell = 2;
    if (cell > 32) cell = 32;

    int total_w = (int)(spr->width * cell);
    int total_h = (int)(spr->height * cell);
    int ox = c.x + (c.w - total_w) / 2;
    int oy = c.y + (c.h - total_h) / 2;

    /* Draw pixels */
    for (int y = 0; y < spr->height; y++) {
        for (int x = 0; x < spr->width; x++) {
            SDL_Color col = QL_SDL_COLORS[spr->pixels[y][x]];
            int rx = ox + (int)(x * cell);
            int ry = oy + (int)(y * cell);
            int rw = (int)((x + 1) * cell) - (int)(x * cell);
            int rh = (int)((y + 1) * cell) - (int)(y * cell);

            SDL_Rect r = {rx, ry, rw, rh};
            SDL_SetRenderDrawColor(app->renderer, col.r, col.g, col.b, 255);
            SDL_RenderFillRect(app->renderer, &r);

            /* Grid */
            SDL_SetRenderDrawColor(app->renderer, ui_theme.canvas_grid.r, ui_theme.canvas_grid.g, ui_theme.canvas_grid.b, 255);
            SDL_RenderDrawRect(app->renderer, &r);
        }
    }

    /* Byte boundary lines (every 2 pixels) */
    SDL_SetRenderDrawColor(app->renderer, ui_theme.canvas_byte.r, ui_theme.canvas_byte.g, ui_theme.canvas_byte.b, 255);
    for (int x = 0; x <= spr->width; x += 2) {
        int lx = ox + (int)(x * cell);
        SDL_RenderDrawLine(app->renderer, lx, oy, lx, oy + total_h);
    }

    /* Mouse interaction */
    if (ui_mouse_in_rect(ox, oy, total_w, total_h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int px_x = (int)((mx - ox) / cell);
        int px_y = (int)((my - oy) / cell);
        if (px_x >= 0 && px_x < spr->width && px_y >= 0 && px_y < spr->height) {
            /* Left click: paint */
            if (ui_mouse_down()) {
                spr->pixels[px_y][px_x] = app->current_color;
                app->modified = 1;
            }
            /* Right click: pick color */
            if (ui_mouse_right_clicked()) {
                app->current_color = spr->pixels[px_y][px_x];
            }
            /* Tooltip */
            char tip[64];
            snprintf(tip, sizeof(tip), "(%d, %d) %d: %s", px_x, px_y,
                     spr->pixels[px_y][px_x], QL_COLOR_NAMES[spr->pixels[px_y][px_x]]);
            ui_tooltip(tip);
        }
    }

    ui_panel_end();
}

/* ── Tools panel ──────────────────────────────────────────── */

static void draw_tools_panel(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int y = c.y;

    /* Palette */
    y = ui_section(c.x - 4, y, c.w + 8, "Palette");

    for (int i = 0; i < 8; i++) {
        int sw = 22;
        if (ui_color_swatch(c.x, y, sw, QL_SDL_COLORS[i], app->current_color == i)) {
            app->current_color = i;
        }
        char label[32];
        snprintf(label, sizeof(label), "%d: %s", i, QL_COLOR_NAMES[i]);
        ui_text(c.x + sw + 6, y + 2, label);
        y += sw + 3;
    }

    /* Number key shortcuts for palette */
    for (int i = 0; i < 8; i++) {
        if (ui_key_pressed(SDLK_0 + i))
            app->current_color = i;
    }

    y += 8;

    /* ── Properties ── */
    if (app->current_sprite < app->sprite_count) {
        Sprite *spr = &app->sprites[app->current_sprite];

        y = ui_section(c.x - 4, y, c.w + 8, "Properties");

        /* Calculate label width for alignment */
        int lw = ui_text_width("Name:");
        int tw_w = ui_text_width("Width:");
        int tw_h = ui_text_width("Height:");
        if (tw_w > lw) lw = tw_w;
        if (tw_h > lw) lw = tw_h;
        int field_x = c.x + lw + 8;
        int field_w = c.w - lw - 8;

        /* Name */
        ui_text_color(c.x, y + 3, "Name:", ui_theme.text_dim);
        strncpy(app->name_buf, spr->name, sizeof(app->name_buf) - 1);
        if (ui_input_text(field_x, y, field_w, app->name_buf, sizeof(app->name_buf), &app->name_focus)) {
            strncpy(spr->name, app->name_buf, sizeof(spr->name) - 1);
        }
        y += ui_line_height() + 10;

        /* Width / Height */
        int w = spr->width;
        if (ui_input_int_ex(c.x, y, c.w, "Width:", lw, &w, 2, 2, MAX_SPRITE_W)) {
            sprite_resize(spr, w, spr->height);
        }
        y += ui_line_height() + 10;

        int h = spr->height;
        if (ui_input_int_ex(c.x, y, c.w, "Height:", lw, &h, 1, 1, MAX_SPRITE_H)) {
            sprite_resize(spr, spr->width, h);
        }
        y += ui_line_height() + 14;

        /* Size info — centered, dark */
        char info[32];
        snprintf(info, sizeof(info), "Size: %d bytes", (spr->width / 2) * spr->height);
        int iw = ui_text_width(info);
        ui_text_color(c.x + (c.w - iw) / 2, y, info, ui_theme.text_dark);
        y += ui_line_height() + 8;

        /* Preview */
        y = ui_section(c.x - 4, y, c.w + 8, "Preview");

        /* Draw preview at 3x */
        int show_idx = app->current_sprite;
        if (app->animate) {
            app->anim_timer += 1.0f / 60.0f; /* approximate */
            if (app->anim_timer > 0.2f) {
                app->anim_timer = 0;
                app->anim_frame++;
            }
            /* Find animation group */
            char *us = strrchr(spr->name, '_');
            if (us) {
                int pfx_len = (int)(us - spr->name + 1);
                int group[MAX_SPRITES];
                int gc = 0;
                for (int i = 0; i < app->sprite_count; i++) {
                    if (strncmp(app->sprites[i].name, spr->name, pfx_len) == 0) {
                        const char *suffix = app->sprites[i].name + pfx_len;
                        if (suffix[0] >= '0' && suffix[0] <= '9')
                            group[gc++] = i;
                    }
                }
                if (gc > 0)
                    show_idx = group[app->anim_frame % gc];
            }
        }

        Sprite *pspr = &app->sprites[show_idx];
        int scale = 3;
        int prev_w = pspr->width * scale;
        int prev_h = pspr->height * scale;

        /* Black background */
        SDL_Rect bg = {c.x, y, prev_w + 4, prev_h + 4};
        SDL_SetRenderDrawColor(app->renderer, ui_theme.preview_bg.r, ui_theme.preview_bg.g, ui_theme.preview_bg.b, 255);
        SDL_RenderFillRect(app->renderer, &bg);

        for (int py2 = 0; py2 < pspr->height; py2++) {
            for (int px2 = 0; px2 < pspr->width; px2++) {
                uint8_t ci = pspr->pixels[py2][px2];
                if (ci == 0) continue;
                SDL_Color col = QL_SDL_COLORS[ci];
                SDL_Rect r = {c.x + 2 + px2 * scale, y + 2 + py2 * scale, scale, scale};
                SDL_SetRenderDrawColor(app->renderer, col.r, col.g, col.b, 255);
                SDL_RenderFillRect(app->renderer, &r);
            }
        }
        y += prev_h + 8;

        ui_checkbox(c.x, y, "Animate", &app->animate);
    }

    ui_panel_end();
}

/* ── Console panel ────────────────────────────────────────── */

static void draw_console(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Console", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Darker background for console content */
    SDL_Rect bg = {c.x - 4, c.y - 2, c.w + 8, c.h + 4};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.console_bg.r, ui_theme.console_bg.g, ui_theme.console_bg.b, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    int y = c.y;
    int max_lines = (c.h) / (ui_line_height() + 1);
    int start = app->console_count - max_lines;
    if (start < 0) start = 0;

    for (int i = start; i < app->console_count; i++) {
        const char *line = app->console_lines[i];
        SDL_Color col = ui_theme.text_dim;
        if (strstr(line, "ERR"))  col = ui_theme.console_err;
        else if (strstr(line, "***")) col = ui_theme.console_warn;
        ui_text_color(c.x, y, line, col);
        y += ui_line_height() + 1;
        if (y > c.y + c.h - ui_line_height()) break;
    }

    if (app->console_count == 0) {
        ui_text_color(c.x, c.y, "Console output appears here.", ui_theme.text_dim);
    }

    ui_panel_end();
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
    int top = TAB_BAR_H;
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
        ui_panel_begin("Image Canvas", cx, top, cw + rw, ch);
        ui_text(cx + 20, top + 40, "Image editor — coming soon");
        ui_panel_end();
    } else if (app->view == VIEW_EMULATOR) {
        int emu_panel_w = cw + rw;
        ui_panel_begin("Emulator", cx, top, emu_panel_w, ch);
        SDL_Rect ec = ui_panel_content();

        /* Emulator screen area — 512x256 scaled to fit */
        float emu_w = 512, emu_h = 256;
        float sx = (float)ec.w / emu_w;
        float sy = (float)(ec.h - 40) / emu_h;
        float s = sx < sy ? sx : sy;
        if (s < 0.1f) s = 1.0f;
        int pw = (int)(emu_w * s);
        int ph = (int)(emu_h * s);
        int scr_x = ec.x + (ec.w - pw) / 2;
        int scr_y = ec.y + 30 + ((ec.h - 30 - ph) / 2);

        /* Dark red placeholder */
        SDL_Rect scr = {scr_x, scr_y, pw, ph};
        SDL_SetRenderDrawColor(app->renderer, ui_theme.emu_placeholder.r, ui_theme.emu_placeholder.g, ui_theme.emu_placeholder.b, 255);
        SDL_RenderFillRect(app->renderer, &scr);

        /* Centered message */
        const char *msg = "Press Build & Run";
        int tw = ui_text_width(msg);
        ui_text_color(scr_x + (pw - tw) / 2, scr_y + ph / 2 - ui_line_height() / 2,
                      msg, ui_theme.emu_text);

        ui_panel_end();
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

/* ── File I/O (JSON — same format as studio 1) ────────────── */

void app_new_project(App *app) {
    app->sprite_count = 1;
    sprite_init(&app->sprites[0], "sprite_0", 10, 20);
    app->current_sprite = 0;
    app->project_path[0] = 0;
    app->modified = 0;
}

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log(app, "ERR Cannot open %s", path); return; }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = (char *)malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    /* Simple JSON parse — find sprites array */
    app->sprite_count = 0;
    const char *p = strstr(json, "\"sprites\"");
    if (!p) { free(json); return; }
    p = strchr(p, '[');
    if (!p) { free(json); return; }
    p++;

    while (*p && app->sprite_count < MAX_SPRITES) {
        /* Skip whitespace/commas */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
        if (*p == ']') break;
        if (*p != '{') break;
        p++;

        Sprite *spr = &app->sprites[app->sprite_count];
        memset(spr, 0, sizeof(*spr));

        while (*p && *p != '}') {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
            if (*p == '}') break;
            if (*p != '"') break;
            p++;
            char key[32] = {};
            int ki = 0;
            while (*p && *p != '"' && ki < 31) key[ki++] = *p++;
            if (*p == '"') p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':') p++;

            if (strcmp(key, "name") == 0) {
                if (*p == '"') p++;
                int ni = 0;
                while (*p && *p != '"' && ni < 63) spr->name[ni++] = *p++;
                if (*p == '"') p++;
            } else if (strcmp(key, "width") == 0) {
                spr->width = atoi(p);
                while ((*p >= '0' && *p <= '9') || *p == '-') p++;
            } else if (strcmp(key, "height") == 0) {
                spr->height = atoi(p);
                while ((*p >= '0' && *p <= '9') || *p == '-') p++;
            } else if (strcmp(key, "pixels") == 0) {
                /* Parse [[0,1,...], ...] */
                if (*p == '[') p++;
                int row = 0;
                while (*p && *p != ']' && row < MAX_SPRITE_H) {
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                    if (*p == ']') break;
                    if (*p != '[') break;
                    p++;
                    int col = 0;
                    while (*p && *p != ']' && col < MAX_SPRITE_W) {
                        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                        if (*p == ']') break;
                        spr->pixels[row][col++] = atoi(p);
                        while ((*p >= '0' && *p <= '9') || *p == '-') p++;
                    }
                    if (*p == ']') p++;
                    row++;
                }
                if (*p == ']') p++;
            }
        }
        if (*p == '}') p++;
        app->sprite_count++;
    }

    free(json);
    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->current_sprite = 0;
    app->modified = 0;
    app_log(app, "Loaded: %s (%d sprites)", path, app->sprite_count);
}

void app_save_project(App *app, const char *path) {
    if (path) strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    if (!app->project_path[0]) return;

    FILE *f = fopen(app->project_path, "w");
    if (!f) return;

    fprintf(f, "{\n  \"sprites\": [\n");
    for (int i = 0; i < app->sprite_count; i++) {
        Sprite *s = &app->sprites[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", s->name);
        fprintf(f, "      \"width\": %d,\n", s->width);
        fprintf(f, "      \"height\": %d,\n", s->height);
        fprintf(f, "      \"pixels\": [\n");
        for (int y = 0; y < s->height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < s->width; x++) {
                fprintf(f, "%d", s->pixels[y][x]);
                if (x < s->width - 1) fprintf(f, ", ");
            }
            fprintf(f, "]%s\n", y < s->height - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i < app->sprite_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    app->modified = 0;
    app_log(app, "Saved: %s (%d sprites)", app->project_path, app->sprite_count);
}

void app_export_c(App *app, const char *directory) {
    char c_path[512], h_path[512];
    snprintf(c_path, sizeof(c_path), "%s/sprites.c", directory);
    snprintf(h_path, sizeof(h_path), "%s/sprites.h", directory);

    FILE *fc = fopen(c_path, "w");
    FILE *fh = fopen(h_path, "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

    fprintf(fc, "/* sprites.c — Generated by QL Studio 2 */\n");
    fprintf(fc, "#include \"sprites.h\"\n\n");

    fprintf(fh, "/* sprites.h — Generated by QL Studio 2 */\n");
    fprintf(fh, "#ifndef SPRITES_H\n#define SPRITES_H\n\n");
    fprintf(fh, "#include \"game.h\"\n\n");
    fprintf(fh, "typedef struct { uint8_t w; uint8_t h; const uint8_t *data; } Sprite;\n\n");

    for (int i = 0; i < app->sprite_count; i++) {
        Sprite *spr = &app->sprites[i];
        fprintf(fc, "/* %s — %dx%d */\n", spr->name, spr->width, spr->height);
        fprintf(fc, "static const uint8_t spr_%s_data[] = {\n", spr->name);
        for (int y = 0; y < spr->height; y++) {
            fprintf(fc, "    ");
            for (int x = 0; x < spr->width; x += 2) {
                uint8_t hi = spr->pixels[y][x];
                uint8_t lo = (x + 1 < spr->width) ? spr->pixels[y][x + 1] : 0;
                fprintf(fc, "0x%02X, ", (hi << 4) | lo);
            }
            fprintf(fc, "\n");
        }
        fprintf(fc, "};\n");
        fprintf(fc, "const Sprite spr_%s = { %d, %d, spr_%s_data };\n\n",
                spr->name, spr->width, spr->height, spr->name);
        fprintf(fh, "extern const Sprite spr_%s;\n", spr->name);
    }

    fprintf(fh, "\n#endif\n");
    fclose(fc);
    fclose(fh);
    app_log(app, "Exported %d sprites to %s", app->sprite_count, directory);
}
