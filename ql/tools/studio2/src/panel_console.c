/*
 * panel_console.c — Console log panel
 */

#include "app.h"
#include "ui.h"
#include <string.h>

void draw_console(App *app, int px, int py, int pw, int ph) {
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
