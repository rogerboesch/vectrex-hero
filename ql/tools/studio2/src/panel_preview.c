/*
 * panel_preview.c — Sprite preview at 3x with animation
 */

#include "app.h"
#include "ui.h"
#include <string.h>

void draw_preview(App *app, int x, int y, int w, int *out_y) {
    if (app->current_sprite >= app->sprite_count) { *out_y = y; return; }
    Sprite *spr = &app->sprites[app->current_sprite];

    y = ui_section(x, y, w, "Preview");

    int cx = x + 4;

    /* Find sprite to show (animation group if animating) */
    int show_idx = app->current_sprite;
    if (app->animate) {
        app->anim_timer += 1.0f / 60.0f;
        if (app->anim_timer > 0.2f) {
            app->anim_timer = 0;
            app->anim_frame++;
        }
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
    SDL_Rect bg = {cx, y, prev_w + 4, prev_h + 4};
    SDL_SetRenderDrawColor(app->renderer, ui_theme.preview_bg.r, ui_theme.preview_bg.g, ui_theme.preview_bg.b, 255);
    SDL_RenderFillRect(app->renderer, &bg);

    for (int py2 = 0; py2 < pspr->height; py2++) {
        for (int px2 = 0; px2 < pspr->width; px2++) {
            uint8_t ci = pspr->pixels[py2][px2];
            if (ci == 0) continue;
            SDL_Color col = QL_SDL_COLORS[ci];
            SDL_Rect r = {cx + 2 + px2 * scale, y + 2 + py2 * scale, scale, scale};
            SDL_SetRenderDrawColor(app->renderer, col.r, col.g, col.b, 255);
            SDL_RenderFillRect(app->renderer, &r);
        }
    }
    y += prev_h + 8;

    ui_checkbox(cx, y, "Animate", &app->animate);
    y += ui_line_height() + 8;

    *out_y = y;
}
