/* panel_emu.c — Stub (coming soon) */
#include "app.h"
#include "ui.h"
void draw_emu_panel(App *app, int x, int y, int w, int h) {
    ui_panel_begin("Emulator", x, y, w, h);
    SDL_Rect c = ui_panel_content();
    /* Dark red placeholder */
    float emu_w = 330, emu_h = 410;
    float sx = (float)c.w / emu_w;
    float sy = (float)c.h / emu_h;
    float s = sx < sy ? sx : sy;
    if (s < 0.1f) s = 1.0f;
    int pw = (int)(emu_w * s), ph = (int)(emu_h * s);
    int scr_x = c.x + (c.w - pw) / 2;
    int scr_y = c.y + (c.h - ph) / 2;
    SDL_Rect scr = {scr_x, scr_y, pw, ph};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.emu_placeholder.r,
                           ui_theme.emu_placeholder.g, ui_theme.emu_placeholder.b, 255);
    SDL_RenderFillRect(app->renderer, &scr);
    const char *msg = "Press Run";
    int tw = ui_text_width(msg);
    ui_text_color(scr_x + (pw - tw) / 2, scr_y + ph / 2, msg, ui_theme.emu_text);
    ui_panel_end();
}
