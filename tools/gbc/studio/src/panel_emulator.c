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
    if (ui_button(bx, tb.y, 50, bh, "Build")) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cd \"%s/../../../gbc\" && make 2>&1", SDL_GetBasePath());
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp)) {
                int len = (int)strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
                app_log_dbg(app, "%s", buf);
            }
            if (pclose(fp) == 0) app_log_info(app, "Build OK");
            else app_log_err(app, "Build FAILED");
        }
    }
    bx += 62;

    if (!gbc_emu_is_running()) {
        if (ui_button(bx, tb.y, 40, bh, "Run")) {
            char rom[512];
            snprintf(rom, sizeof(rom), "%s/../../../gbc/hero.gbc", SDL_GetBasePath());
            if (gbc_emu_load(app->renderer, rom))
                app_log_info(app, "GBC emulator started");
            else
                app_log_err(app, "Failed to load ROM");
        }
    } else {
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
