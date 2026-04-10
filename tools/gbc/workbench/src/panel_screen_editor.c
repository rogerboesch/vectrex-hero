/*
 * panel_screen_editor.c — Fixed 20x18 screen editor (title, game over, etc.)
 */
#include "app.h"
#include "ui.h"
#include "style.h"

#define TILE_SIZE 8
#define ZOOM 3

void draw_screen_editor(App *app, int x, int y, int w, int h) {
    int grid_w = SCREEN_W * TILE_SIZE * ZOOM;
    int grid_h = SCREEN_H * TILE_SIZE * ZOOM;
    int ox = x + (w - grid_w) / 2;
    int oy = y + (h - grid_h) / 2;
    if (ox < x) ox = x;
    if (oy < y) oy = y;

    /* Ensure at least one screen exists */
    if (app->tmap.screen_count == 0) {
        app->tmap.screen_count = 1;
        memset(&app->tmap.screens[0], 0, sizeof(GameScreen));
        strcpy(app->tmap.screens[0].name, "title");
    }
    GameScreen *scr = &app->tmap.screens[app->cur_screen];

    /* Screen selector buttons at top */
    {
        int bx = ox;
        for (int i = 0; i < app->tmap.screen_count; i++) {
            if (ui_button(bx, oy - 24, 80, 20, app->tmap.screens[i].name)) {
                app->cur_screen = i;
                scr = &app->tmap.screens[app->cur_screen];
            }
            bx += 84;
        }
        /* Add screen button */
        if (app->tmap.screen_count < MAX_SCREENS) {
            if (ui_button(bx, oy - 24, 24, 20, "+")) {
                int idx = app->tmap.screen_count++;
                memset(&app->tmap.screens[idx], 0, sizeof(GameScreen));
                snprintf(app->tmap.screens[idx].name,
                         sizeof(app->tmap.screens[idx].name),
                         "screen_%d", idx);
                app->cur_screen = idx;
                scr = &app->tmap.screens[app->cur_screen];
                app->modified = true;
            }
        }

        /* DMG toggle */
        int dbx = ox + grid_w - 24;
        if (ui_button(dbx, oy - 24, 24, 20, "")) { app->dmg_preview = !app->dmg_preview; }
        ui_icon_centered(dbx, oy - 24, 24, 20, ICON_EYE,
                         app->dmg_preview ? (SDL_Color){0,200,0,255} : ui_theme.text);
    }

    /* Draw the 20x18 grid with tiles */
    for (int ty = 0; ty < SCREEN_H; ty++) {
        for (int tx = 0; tx < SCREEN_W; tx++) {
            int px = ox + tx * TILE_SIZE * ZOOM;
            int py = oy + ty * TILE_SIZE * ZOOM;
            uint8_t tile_idx = scr->tiles[ty][tx];

            /* Render tile from tileset */
            if (tile_idx < app->tmap.tileset.used_count) {
                TilesetEntry *te = &app->tmap.tileset.entries[tile_idx];
                GBCPalette *pal = &app->tmap.bg_pals[te->palette < MAX_BG_PALS ? te->palette : 0];
                for (int py2 = 0; py2 < 8; py2++) {
                    for (int px2 = 0; px2 < 8; px2++) {
                        uint8_t ci = tset_get_pixel(te, px2, py2);
                        uint8_t r8, g8, b8;
                        if (app->dmg_preview)
                            dmg_color(ci, &r8, &g8, &b8);
                        else {
                            RGB5 rgb = pal->colors[ci];
                            rgb5_to_rgb8(rgb, &r8, &g8, &b8);
                        }
                        SDL_Rect rc = { px + px2 * ZOOM, py + py2 * ZOOM, ZOOM, ZOOM };
                        SDL_SetRenderDrawColor(app->renderer, r8, g8, b8, 255);
                        SDL_RenderFillRect(app->renderer, &rc);
                    }
                }
            }
            else {
                /* Empty/black */
                SDL_Rect rc = { px, py, TILE_SIZE * ZOOM, TILE_SIZE * ZOOM };
                SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(app->renderer, &rc);
            }
        }
    }

    /* Grid lines */
    SDL_SetRenderDrawColor(app->renderer, 40, 40, 40, 255);
    for (int tx = 0; tx <= SCREEN_W; tx++) {
        int lx = ox + tx * TILE_SIZE * ZOOM;
        SDL_RenderDrawLine(app->renderer, lx, oy, lx, oy + grid_h);
    }
    for (int ty = 0; ty <= SCREEN_H; ty++) {
        int ly = oy + ty * TILE_SIZE * ZOOM;
        SDL_RenderDrawLine(app->renderer, ox, ly, ox + grid_w, ly);
    }

    /* Mouse interaction: paint tiles or flood fill */
    if (ui_mouse_in_rect(ox, oy, grid_w, grid_h)) {
        int mx, my; ui_mouse_pos(&mx, &my);
        int tx = (mx - ox) / (TILE_SIZE * ZOOM);
        int ty = (my - oy) / (TILE_SIZE * ZOOM);
        if (tx >= 0 && tx < SCREEN_W && ty >= 0 && ty < SCREEN_H) {
            bool ctrl = (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) != 0;
            if (ui_mouse_clicked() && ctrl) {
                /* Ctrl/Cmd+click: flood fill */
                uint8_t old_tile = scr->tiles[ty][tx];
                uint8_t new_tile = (uint8_t)app->cur_tset_tile;
                if (old_tile != new_tile) {
                    static int16_t stk[1024][2];
                    int sp = 0;
                    stk[sp][0] = tx; stk[sp][1] = ty; sp++;
                    while (sp > 0) {
                        sp--;
                        int fx = stk[sp][0], fy = stk[sp][1];
                        if (fx < 0 || fx >= SCREEN_W || fy < 0 || fy >= SCREEN_H) continue;
                        if (scr->tiles[fy][fx] != old_tile) continue;
                        scr->tiles[fy][fx] = new_tile;
                        if (sp + 4 < 1024) {
                            stk[sp][0]=fx+1; stk[sp][1]=fy; sp++;
                            stk[sp][0]=fx-1; stk[sp][1]=fy; sp++;
                            stk[sp][0]=fx; stk[sp][1]=fy+1; sp++;
                            stk[sp][0]=fx; stk[sp][1]=fy-1; sp++;
                        }
                    }
                    app->modified = true;
                }
            }
            else if (ui_mouse_down() && !ctrl) {
                scr->tiles[ty][tx] = (uint8_t)app->cur_tset_tile;
                app->modified = true;
            }
        }
    }
}
