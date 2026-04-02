/*
 * panel_console.c — Console log panel
 */

#include "app.h"
#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

void draw_console(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    /* Toolbar: title + buttons */
    ui_text_color(tb.x, tb.y + 2, "Console", ui_theme.text_dim);

    int btn_w = 50;
    int btn_x = tb.x + tb.w - btn_w;
    if (ui_button(btn_x, tb.y, btn_w, tb.h, "Save")) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char filename[128];
        snprintf(filename, sizeof(filename), "qlstudio-%04d%02d%02d-%02d%02d%02d.log",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
        FILE *f = fopen(filename, "w");
        if (f) {
            for (int i = 0; i < app->console_count; i++)
                fprintf(f, "%s\n", app->console_lines[i]);
            fclose(f);
            app_log(app, "Log saved: %s", filename);
        }
    }
    btn_x -= btn_w + 4;
    if (ui_button(btn_x, tb.y, btn_w, tb.h, "Clear")) {
        app->console_count = 0;
    }

    /* Darker background for console content */
    SDL_Rect bg = {c.x - 4, c.y - 2, c.w + 8, c.h + 4};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.console_bg.r, ui_theme.console_bg.g, ui_theme.console_bg.b, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    /* Always show last 10 messages */
    int y = c.y;
    int max_lines = (c.h) / (ui_line_height() + 1);
    if (max_lines > 10) max_lines = 10;
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
