/*
 * panel_emulator.c — GBC emulator panel
 */
#include "app.h"
#include "ui.h"
#include "gbc_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool emu_keys_active = false;

void draw_gbc_emulator(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    int bx = tb.x, bh = tb.h;

    /* Build */
    if (!app_build_is_active(app)) {
        if (ui_button_accent(bx, tb.y, 50, bh, "Build")) {
            app_build_start(app, false);
        }
    }
    else {
        ui_button(bx, tb.y, 50, bh, "...");
    }
    bx += 62;

    if (!gbc_emu_is_running()) {
        if (ui_button(bx, tb.y, 40, bh, "Run")) {
            if (!app->build_dir[0] || !app->rom_name[0]) {
                app_log_err(app, "Set build_dir and rom_name in game-rescue.json");
            }
            else {
                char rom[512];
                snprintf(rom, sizeof(rom), "%s/%s", app->build_dir, app->rom_name);
                if (gbc_emu_load(app->renderer, rom))
                    app_log_info(app, "Emulator: %s", rom);
                else
                    app_log_err(app, "ROM not found: %s", rom);
            }
        }
    } else {
        if (ui_button(bx, tb.y, 55, bh, gbc_emu_is_paused() ? "Resume" : "Pause")) {
            if (gbc_emu_is_paused()) gbc_emu_resume(); else gbc_emu_pause();
        }
        bx += 59;
        if (ui_button(bx, tb.y, 55, bh, "Reload")) {
            gbc_emu_stop();
            char rom[512];
            snprintf(rom, sizeof(rom), "%s/%s", app->build_dir, app->rom_name);
            if (gbc_emu_load(app->renderer, rom))
                app_log_info(app, "Reloaded: %s", rom);
            else
                app_log_err(app, "ROM not found: %s", rom);
        }
        bx += 59;
        /* Status */
        GbcCpuState cpu = gbc_emu_get_cpu();
        char status[48];
        snprintf(status, sizeof(status), "PC:%04X SP:%04X", cpu.pc, cpu.sp);
        ui_text_mono_color(tb.x + tb.w - 160, tb.y + 2, status, ui_theme.text_dim);
    }

    /* Display area */
    SDL_Rect scr = {c.x, c.y, c.w, c.h};
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &scr);

    if (gbc_emu_is_running()) {
        gbc_emu_step();
        int bp = gbc_emu_get_last_bp();
        if (bp > 0) app_log_warn(app, "BREAKPOINT hit at $%04X", bp - 1);
        gbc_emu_render(app->renderer, c.x, c.y, c.w, c.h);
    } else {
        const char *msg = "Press Run";
        int tw = ui_text_width(msg);
        ui_text_color(c.x + (c.w - tw) / 2, c.y + c.h / 2, msg, ui_theme.emu_text);
    }

    emu_keys_active = gbc_emu_is_running() && ui_mouse_in_rect(c.x, c.y, c.w, c.h);
    ui_panel_end();
}

bool gbc_emu_wants_keys(void) { return emu_keys_active; }
