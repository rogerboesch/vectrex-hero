/*
 * panel_emulator.c — Emulator display with toolbar + pixel inspector
 */

#include "app.h"
#include "ui.h"
#include "emulator.h"
#include "sprite.h"  /* QL_COLOR_NAMES */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *speed_names[] = {"Fast", "Normal", "Slow", "V.Slow"};
static int speed_values[] = {0, 1, 4, 10};

/*
 * Parse an iQL log line and re-log with our format.
 * Input:  "[HH:MM:SS.mmm] INF (file.c:123) message"
 * Output: "> message (file.c:123)"  for DBG
 *         "* message"               for INF
 *         "! message"               for ERR
 */
static void log_emu_line(App *app, const char *line) {
    /* Find level marker after "] " */
    const char *p = strstr(line, "] ");
    if (!p) { app_log_dbg(app, "%s", line); return; }
    p += 2; /* skip "] " */

    /* Extract level (3 chars) */
    char level[4] = {};
    strncpy(level, p, 3);
    p += 3;

    /* Skip " (" to find filename:line */
    char fileline[64] = {};
    if (*p == ' ' && *(p+1) == '(') {
        p += 2;
        const char *end = strchr(p, ')');
        if (end) {
            int len = (int)(end - p);
            if (len > 63) len = 63;
            strncpy(fileline, p, len);
            p = end + 1;
        }
    }

    /* Skip space before message */
    while (*p == ' ') p++;

    if (strcmp(level, "DBG") == 0) {
        if (fileline[0])
            app_log_dbg(app, "%s (%s)", p, fileline);
        else
            app_log_dbg(app, "%s", p);
    } else if (strcmp(level, "ERR") == 0) {
        app_log_err(app, "%s", p);
    } else {
        app_log_info(app, "%s", p);
    }
}

/* Drain a log buffer, split by newlines, parse each line */
static void drain_and_log(App *app, int (*drain_fn)(char *, int), bool parse_emu) {
    char logbuf[8192];
    int n = drain_fn(logbuf, sizeof(logbuf));
    if (n <= 0) return;
    char *p = logbuf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        if (*p) {
            if (parse_emu) log_emu_line(app, p);
            else app_log_dbg(app, "%s", p);
        }
        if (nl) p = nl + 1; else break;
    }
}

void draw_emulator(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar */
    int bx = tb.x;
    int bh = tb.h;

    /* Build — always available */
    if (ui_button(bx, tb.y, 50, bh, "Build")) {
        char ql_dir[512];
        snprintf(ql_dir, sizeof(ql_dir), "%s../../../ql/src/", SDL_GetBasePath());
        char output[4096] = {};
        app_log_info(app, "Building...");
        if (emu_build(ql_dir, output, sizeof(output)))
            app_log_info(app, "Build OK");
        else
            app_log_err(app, "Build FAILED");
        if (output[0]) app_log_dbg(app, "%s", output);
    }
    bx += 62;

    if (!emu_is_running()) {
        if (ui_button(bx, tb.y, 40, bh, "Run")) {
            char sys_path[512];
            snprintf(sys_path, sizeof(sys_path), "%s/Documents/iQLmac/", getenv("HOME"));
            if (emu_start(app->renderer, sys_path)) {
                app_log_info(app, "Emulator started");
                app->emu_start_ticks = SDL_GetTicks();
                app->soft_bp_armed = false;
            }
        }
    } else {
        if (ui_button(bx, tb.y, 55, bh, emu_is_paused() ? "Resume" : "Pause")) {
            if (emu_is_paused()) emu_resume(); else emu_pause();
        }
        bx += 59;
        if (ui_button(bx, tb.y, 55, bh, "Restart")) { emu_restart(); }
        bx += 59;
        if (ui_button(bx, tb.y, 55, bh, speed_names[app->speed_idx])) {
            app->speed_idx = (app->speed_idx + 1) % 4;
            emu_set_speed(speed_values[app->speed_idx]);
        }
        bx += 75;
        if (ui_checkbox(bx, tb.y + 2, "Log OS Traps", &app->trap_log_enabled))
            emu_set_trap_logging(app->trap_log_enabled);

        /* Status */
        bx = tb.x + tb.w - 70;
        if (emu_is_paused())
            ui_text_color(bx, tb.y + 2, "Paused", (SDL_Color){255, 200, 50, 255});
        else
            ui_text_color(bx, tb.y + 2, "Running", (SDL_Color){80, 200, 80, 255});
    }

    /* Boot delay for software breakpoints */
    if (emu_is_running() && !app->soft_bp_armed && app->bp_enabled &&
        SDL_GetTicks() - app->emu_start_ticks > 3000) {
        emu_set_soft_bp_enabled(true);
        app->soft_bp_armed = true;
        app_log_warn(app, "Software breakpoints enabled (after boot delay)");
    }

    /* Display area */
    if (emu_is_running()) emu_update_texture();

    float dw = 512, dh = 256;
    if (emu_is_ready()) {
        int sw = emu_screen_width(), sh = emu_screen_height();
        if (sw > 0 && sh > 0) { dw = (float)sw; dh = (float)sh; }
    }

    float scale = (float)c.w / dw;
    float sy = (float)(c.h - 20) / dh;
    if (sy < scale) scale = sy;
    if (scale < 0.1f) scale = 1.0f;
    int disp_w = (int)(dw * scale);
    int disp_h = (int)(dh * scale);
    int scr_x = c.x + (c.w - disp_w) / 2;
    int scr_y = c.y + (c.h - 20 - disp_h) / 2;

    SDL_Texture *tex = emu_get_texture();
    if (emu_is_ready() && tex) {
        SDL_Rect dst = {scr_x, scr_y, disp_w, disp_h};
        SDL_RenderCopy(app->renderer, tex, NULL, &dst);

        /* Pixel inspector on hover */
        if (ui_mouse_in_rect(scr_x, scr_y, disp_w, disp_h)) {
            int mx, my;
            ui_mouse_pos(&mx, &my);
            int qx = (int)((mx - scr_x) / scale);
            int qy = (int)((my - scr_y) / scale);
            int ql_x = qx / 2;
            int ql_y = qy;
            if (ql_x >= 0 && ql_x < 256 && ql_y >= 0 && ql_y < 256) {
                uint32_t pair_addr = 0x20000 + ql_y * 128 + (ql_x / 4) * 2;
                int pos = ql_x & 3;
                uint8_t even = emu_read_byte(pair_addr);
                uint8_t odd = emu_read_byte(pair_addr + 1);
                int shift = (3 - pos) * 2;
                int g = (even >> (shift + 1)) & 1;
                int r = (odd >> shift) & 1;
                int b = (odd >> (shift + 1)) & 1;
                int color = (g << 2) | (b << 1) | r;
                char info[80];
                snprintf(info, sizeof(info), "X:%d Y:%d  Color:%d (%s)  $%05X  E:$%02X O:$%02X",
                         ql_x, ql_y, color, color < 8 ? QL_COLOR_NAMES[color] : "?",
                         pair_addr, even, odd);
                ui_text_color(c.x, c.y + c.h - 16, info, ui_theme.text_dim);
            }
        }
    } else {
        /* Dark red placeholder */
        SDL_Rect scr = {scr_x, scr_y, disp_w, disp_h};
        SDL_SetRenderDrawColor(app->renderer, ui_theme.emu_placeholder.r,
                               ui_theme.emu_placeholder.g, ui_theme.emu_placeholder.b, 255);
        SDL_RenderFillRect(app->renderer, &scr);
        const char *msg = emu_is_running() ? "Booting..." : "Press Run";
        int tw = ui_text_width(msg);
        ui_text_color(scr_x + (disp_w - tw) / 2, scr_y + disp_h / 2 - ui_line_height() / 2,
                      msg, ui_theme.emu_text);
    }

    /* Check software breakpoint hit */
    int bp_hit = emu_get_last_bp_hit();
    if (bp_hit != 0) {
        if (bp_hit > 0)
            app_log_warn(app, "SOFTWARE BP #%d hit", bp_hit);
        else
            app_log_warn(app, "BREAKPOINT hit at $%06X", (unsigned)(-bp_hit));
    }

    /* Drain iQL log + trap log to console */
    if (emu_is_running()) {
        drain_and_log(app, emu_drain_iql_log, true);
        if (app->trap_log_enabled)
            drain_and_log(app, emu_drain_trap_log, false);
    }

    /* Auto-route keyboard when emulator panel is hovered */
    app->emu_wants_keys = emu_is_running() && !emu_is_paused() &&
                          ui_mouse_in_rect(px, py, pw, ph);

    ui_panel_end();
}
