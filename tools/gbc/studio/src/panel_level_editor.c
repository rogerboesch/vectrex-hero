/*
 * panel_level_editor.c — Scrollable tilemap canvas
 *
 * Shows the tilemap for the current level. Left-click paints selected tile.
 * Right-click picks tile. Middle-click/drag to scroll.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>

void draw_level_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    if (app->cur_level < 0 || app->cur_level >= app->tmap.level_count) { ui_panel_end(); return; }
    TilemapLevel *lvl = &app->tmap.levels[app->cur_level];
    Tileset *ts = &app->tmap.tileset;
    GBCPalette *pal = &app->tmap.bg_pals[0];

    /* Toolbar info */
    char info[64];
    snprintf(info, sizeof(info), "%s  %dx%d  Tile:%d", lvl->name, lvl->width, lvl->height, app->cur_tset_tile);
    ui_text_color(tb.x, tb.y + 2, info, ui_theme.text_dim);

    /* Cell size */
    int cell = 8;  /* 1:1 pixel mapping */

    /* Visible area */
    int vis_w = c.w / cell, vis_h = c.h / cell;
    if (vis_w > lvl->width) vis_w = lvl->width;
    if (vis_h > lvl->height) vis_h = lvl->height;

    /* Clamp scroll */
    if (app->scroll_x > lvl->width - vis_w) app->scroll_x = lvl->width - vis_w;
    if (app->scroll_y > lvl->height - vis_h) app->scroll_y = lvl->height - vis_h;
    if (app->scroll_x < 0) app->scroll_x = 0;
    if (app->scroll_y < 0) app->scroll_y = 0;

    /* Center if map fits */
    int ox = c.x, oy = c.y;
    if (lvl->width * cell <= c.w) ox = c.x + (c.w - lvl->width * cell) / 2;
    if (lvl->height * cell <= c.h) oy = c.y + (c.h - lvl->height * cell) / 2;

    /* Draw visible tiles */
    for (int ty = 0; ty < vis_h; ty++) {
        for (int tx = 0; tx < vis_w; tx++) {
            int map_x = tx + app->scroll_x;
            int map_y = ty + app->scroll_y;
            if (map_x >= lvl->width || map_y >= lvl->height) continue;
            uint8_t tile_idx = lvl->tiles[map_y][map_x];

            int dx = ox + tx * cell, dy = oy + ty * cell;
            if (tile_idx < ts->used_count) {
                TilesetEntry *te = &ts->entries[tile_idx];
                for (int py2 = 0; py2 < 8; py2++) {
                    for (int px2 = 0; px2 < 8; px2++) {
                        uint8_t ci = tset_get_pixel(te, px2, py2);
                        RGB5 rgb = pal->colors[ci];
                        uint8_t r, g, b;
                        rgb5_to_rgb8(rgb, &r, &g, &b);
                        SDL_Rect pr = {dx + px2, dy + py2, 1, 1};
                        SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                        SDL_RenderFillRect(app->renderer, &pr);
                    }
                }
            } else {
                /* Unknown tile = magenta */
                SDL_Rect tr = {dx, dy, cell, cell};
                SDL_SetRenderDrawColor(app->renderer, 200, 0, 200, 255);
                SDL_RenderFillRect(app->renderer, &tr);
            }
        }
    }

    /* Grid overlay (subtle) */
    SDL_SetRenderDrawColor(app->renderer, 30, 30, 40, 60);
    int draw_w = vis_w * cell, draw_h = vis_h * cell;
    for (int tx = 0; tx <= vis_w; tx++)
        SDL_RenderDrawLine(app->renderer, ox + tx * cell, oy, ox + tx * cell, oy + draw_h);
    for (int ty = 0; ty <= vis_h; ty++)
        SDL_RenderDrawLine(app->renderer, ox, oy + ty * cell, ox + draw_w, oy + ty * cell);

    /* Mouse interaction */
    if (ui_mouse_in_rect(ox, oy, draw_w, draw_h)) {
        int mx, my; ui_mouse_pos(&mx, &my);
        int tx = (mx - ox) / cell + app->scroll_x;
        int ty = (my - oy) / cell + app->scroll_y;

        if (tx >= 0 && tx < lvl->width && ty >= 0 && ty < lvl->height) {
            char tip[32];
            snprintf(tip, sizeof(tip), "[%d,%d] tile:%d", tx, ty, lvl->tiles[ty][tx]);
            ui_tooltip(tip);

            /* Left click/drag: paint current tile */
            if (ui_mouse_down()) {
                lvl->tiles[ty][tx] = (uint8_t)app->cur_tset_tile;
                app->modified = true;
            }
            /* Right click: pick tile */
            if (ui_mouse_right_clicked()) {
                app->cur_tset_tile = lvl->tiles[ty][tx];
            }
        }
    }

    /* Keyboard scroll */
    if (ui_key_pressed(SDLK_LEFT))  app->scroll_x -= 4;
    if (ui_key_pressed(SDLK_RIGHT)) app->scroll_x += 4;
    if (ui_key_pressed(SDLK_UP))    app->scroll_y -= 4;
    if (ui_key_pressed(SDLK_DOWN))  app->scroll_y += 4;

    ui_panel_end();
}
