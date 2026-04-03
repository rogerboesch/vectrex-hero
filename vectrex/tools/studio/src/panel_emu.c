/*
 * panel_emu.c — Vectrex emulator panel with Build/Run/Pause controls
 */

#include "app.h"
#include "ui.h"
#include "vectrex_emu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Track keyboard forwarding */
static bool emu_keys_active = false;

void draw_emu_panel(App *app, int x, int y, int w, int h) {
    SDL_Rect tb = ui_panel_begin_toolbar(x, y, w, h);
    SDL_Rect c = ui_panel_content();

    /* Toolbar */
    int bx = tb.x;
    int bh = tb.h;

    /* Build */
    if (ui_button(bx, tb.y, 50, bh, "Build")) {
        /* Run make in vectrex/ directory */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cd \"%s/../..\" && make 2>&1", SDL_GetBasePath());
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char buf[256];
            bool ok = true;
            while (fgets(buf, sizeof(buf), fp)) {
                /* Trim newline */
                int len = (int)strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
                app_log_dbg(app, "%s", buf);
            }
            if (pclose(fp) == 0)
                app_log_info(app, "Build OK");
            else
                app_log_err(app, "Build FAILED");
        }
    }
    bx += 62;

    if (!vemu_is_running()) {
        if (ui_button(bx, tb.y, 40, bh, "Run")) {
            /* Find ROM and cart */
            char rom_path[512], cart_path[512];
            snprintf(rom_path, sizeof(rom_path), "%s/retro-tools/vectrec/roms/romfast.bin", getenv("HOME"));
            snprintf(cart_path, sizeof(cart_path), "%s/../../bin/main.bin", SDL_GetBasePath());

            if (vemu_load(rom_path, cart_path)) {
                app_log_info(app, "Emulator started");
            } else {
                app_log_err(app, "Failed to load ROM/cart");
            }
        }
    } else {
        if (ui_button(bx, tb.y, 50, bh, "Reset")) {
            vemu_reset();
            app_log_info(app, "Emulator reset");
        }

        /* Status */
        VemuCpuState cpu = vemu_get_cpu();
        char status[64];
        snprintf(status, sizeof(status), "PC:%04X  Vectors:%ld", cpu.pc, vemu_get_vector_count());
        ui_text_mono_color(tb.x + tb.w - 250, tb.y + 2, status, ui_theme.text_dim);
    }

    /* Emulator display area */
    float emu_w = 330, emu_h = 410;
    float sx = (float)c.w / emu_w;
    float sy = (float)c.h / emu_h;
    float s = sx < sy ? sx : sy;
    if (s < 0.1f) s = 1.0f;
    int pw = (int)(emu_w * s), ph = (int)(emu_h * s);
    int scr_x = c.x + (c.w - pw) / 2;
    int scr_y = c.y + (c.h - ph) / 2;

    /* Black background */
    SDL_Rect scr = {scr_x, scr_y, pw, ph};
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &scr);

    if (vemu_is_running()) {
        /* Run one frame */
        vemu_step();
        /* Render vectors */
        vemu_render(app->renderer, scr_x, scr_y, pw, ph);
    } else {
        /* Placeholder */
        SDL_SetRenderDrawColor(app->renderer, ui_theme.emu_placeholder.r,
                               ui_theme.emu_placeholder.g, ui_theme.emu_placeholder.b, 255);
        SDL_RenderFillRect(app->renderer, &scr);
        const char *msg = "Press Run";
        int tw = ui_text_width(msg);
        ui_text_color(scr_x + (pw - tw) / 2, scr_y + ph / 2, msg, ui_theme.emu_text);
    }

    /* Keyboard forwarding when hovering emulator */
    emu_keys_active = vemu_is_running() && ui_mouse_in_rect(scr_x, scr_y, pw, ph);

    ui_panel_end();
}

/* Called from main.c event loop */
bool vectrex_emu_wants_keys(void) {
    return emu_keys_active;
}
