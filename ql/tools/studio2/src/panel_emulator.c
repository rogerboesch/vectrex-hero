/*
 * panel_emulator.c — Emulator view (placeholder for now)
 */

#include "app.h"
#include "ui.h"

void draw_emulator(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Emulator", px, py, pw, ph);
    SDL_Rect ec = ui_panel_content();

    /* Emulator screen area — 512x256 scaled to fit */
    float emu_w = 512, emu_h = 256;
    float sx = (float)ec.w / emu_w;
    float sy = (float)(ec.h - 40) / emu_h;
    float s = sx < sy ? sx : sy;
    if (s < 0.1f) s = 1.0f;
    int pw2 = (int)(emu_w * s);
    int ph2 = (int)(emu_h * s);
    int scr_x = ec.x + (ec.w - pw2) / 2;
    int scr_y = ec.y + 30 + ((ec.h - 30 - ph2) / 2);

    /* Dark red placeholder */
    SDL_Rect scr = {scr_x, scr_y, pw2, ph2};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.emu_placeholder.r, ui_theme.emu_placeholder.g, ui_theme.emu_placeholder.b, 255);
    SDL_RenderFillRect(app->renderer, &scr);

    /* Centered message */
    const char *msg = "Press Build & Run";
    int tw = ui_text_width(msg);
    ui_text_color(scr_x + (pw2 - tw) / 2, scr_y + ph2 / 2 - ui_line_height() / 2,
                  msg, ui_theme.emu_text);

    ui_panel_end();
}
