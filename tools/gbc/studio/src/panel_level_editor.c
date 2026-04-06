/*
 * panel_level_editor.c — Scrollable tilemap canvas
 *
 * Shows the tilemap for the current level. Left-click paints selected tile.
 * Right-click picks tile. Arrow keys, WASD, PgUp/PgDn, mouse wheel to scroll.
 */
#include "app.h"
#include "ui.h"
#include "style.h"
#include <stdio.h>
#include <string.h>

static int zoom = 2;  /* 1=8px, 2=16px, 3=24px per tile */

void draw_level_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    if (app->cur_level < 0 || app->cur_level >= app->tmap.level_count) { ui_panel_end(); return; }
    TilemapLevel *lvl = &app->tmap.levels[app->cur_level];
    Tileset *ts = &app->tmap.tileset;
    GBCPalette *pal = &app->tmap.bg_pals[0];

    /* Toolbar: clear + zoom icons */
    {
        int bw = tb.h;
        int zbx = tb.x + tb.w - 2 * (bw + 2);

        /* Clear button before zoom, with gap */
        int cbx = zbx - bw - 10;
        if (ui_button(cbx, tb.y, bw, tb.h, "")) {
            memset(lvl->tiles, 0, sizeof(lvl->tiles));
            lvl->entity_count = 0;
            app->modified = true;
        }
        ui_icon_centered(cbx, tb.y, bw, tb.h, ICON_ERASER, ui_theme.text);

        if (ui_button(zbx, tb.y, bw, tb.h, "")) { if (zoom > 1) zoom--; }
        ui_icon_centered(zbx, tb.y, bw, tb.h, ICON_ZOOM_OUT, ui_theme.text);
        zbx += bw + 2;
        if (ui_button(zbx, tb.y, bw, tb.h, "")) { if (zoom < 4) zoom++; }
        ui_icon_centered(zbx, tb.y, bw, tb.h, ICON_ZOOM_IN, ui_theme.text);

        /* Zoom label in toolbar */
        char info[32];
        snprintf(info, sizeof(info), "Zoom %d", zoom);
        ui_text_color(tb.x + 4, tb.y + 2, info, ui_theme.text_dim);
    }

    /* Cell size in pixels */
    int cell = 8 * zoom;

    /* Full canvas (scrollbars overlay, no space reserved) */
    int canvas_w = c.w;
    int canvas_h = c.h;

    /* Visible tiles */
    int vis_w = canvas_w / cell;
    int vis_h = canvas_h / cell;
    if (vis_w > lvl->width) vis_w = lvl->width;
    if (vis_h > lvl->height) vis_h = lvl->height;
    if (vis_w < 1) vis_w = 1;
    if (vis_h < 1) vis_h = 1;

    /* Scroll with WASD or arrow keys */
    int scroll_speed = 2;
    if (ui_key_pressed(SDLK_LEFT)  || ui_key_pressed(SDLK_a)) app->scroll_x -= scroll_speed;
    if (ui_key_pressed(SDLK_RIGHT) || ui_key_pressed(SDLK_d)) app->scroll_x += scroll_speed;
    if (ui_key_pressed(SDLK_UP)    || ui_key_pressed(SDLK_w)) app->scroll_y -= scroll_speed;
    if (ui_key_pressed(SDLK_DOWN)  || ui_key_pressed(SDLK_s)) app->scroll_y += scroll_speed;

    /* PgUp / PgDn */
    int page_size = vis_h > 2 ? vis_h - 2 : 1;
    if (ui_key_pressed(SDLK_PAGEUP))   app->scroll_y -= page_size;
    if (ui_key_pressed(SDLK_PAGEDOWN)) app->scroll_y += page_size;
    if (ui_key_pressed(SDLK_HOME))     { app->scroll_x = 0; app->scroll_y = 0; }
    if (ui_key_pressed(SDLK_END))      app->scroll_y = lvl->height;

    /* Mouse wheel (when hovering canvas) */
    if (ui_mouse_in_rect(c.x, c.y, canvas_w, canvas_h)) {
        int wx, wy;
        ui_mouse_wheel(&wx, &wy);
        app->scroll_y -= wy * 3;
        app->scroll_x -= wx * 3;
    }

    /* Clamp scroll */
    int max_sx = lvl->width - vis_w;
    int max_sy = lvl->height - vis_h;
    if (max_sx < 0) max_sx = 0;
    if (max_sy < 0) max_sy = 0;
    if (app->scroll_x > max_sx) app->scroll_x = max_sx;
    if (app->scroll_y > max_sy) app->scroll_y = max_sy;
    if (app->scroll_x < 0) app->scroll_x = 0;
    if (app->scroll_y < 0) app->scroll_y = 0;

    /* Draw position */
    int draw_w = vis_w * cell, draw_h = vis_h * cell;
    int ox = c.x;
    int oy = c.y;

    /* Black background behind map */
    SDL_Rect bg = {ox, oy, draw_w, draw_h};
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(app->renderer, &bg);

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
                        SDL_Rect pr = {dx + px2 * zoom, dy + py2 * zoom, zoom, zoom};
                        SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
                        SDL_RenderFillRect(app->renderer, &pr);
                    }
                }
            } else {
                SDL_Rect tr = {dx, dy, cell, cell};
                SDL_SetRenderDrawColor(app->renderer, 200, 0, 200, 255);
                SDL_RenderFillRect(app->renderer, &tr);
            }
        }
    }

    /* Grid overlay */
    if (zoom >= 2) {
        SDL_SetRenderDrawColor(app->renderer, 40, 40, 50, 255);
        for (int tx = 0; tx <= vis_w; tx++)
            SDL_RenderDrawLine(app->renderer, ox + tx * cell, oy, ox + tx * cell, oy + draw_h);
        for (int ty = 0; ty <= vis_h; ty++)
            SDL_RenderDrawLine(app->renderer, ox, oy + ty * cell, ox + draw_w, oy + ty * cell);
    }

    /* Draw entities (sprite layer) on top */
    {
        static const SDL_Color ent_colors[] = {
            {65, 250, 65, 255},   /* player — green */
            {250, 65, 30, 255},   /* bat — red */
            {210, 50, 210, 255},  /* spider — magenta */
            {50, 200, 50, 255},   /* snake — green-dark */
            {0, 200, 200, 255},   /* miner — cyan */
        };
        static const char *ent_labels[] = {"P", "B", "S", "N", "M"};

        for (int i = 0; i < lvl->entity_count; i++) {
            TilemapEntity *e = &lvl->entities[i];
            int ex = e->x - app->scroll_x, ey = e->y - app->scroll_y;
            if (ex < 0 || ex >= vis_w || ey < 0 || ey >= vis_h) continue;
            int dx = ox + ex * cell, dy = oy + ey * cell;

            SDL_Color ec = (app->sel_entity == i) ? (SDL_Color){255,255,0,255} : ent_colors[e->type % ENT_TYPE_COUNT];
            SDL_SetRenderDrawColor(app->renderer, ec.r, ec.g, ec.b, 255);
            SDL_Rect er = {dx + 2, dy + 2, cell - 4, cell - 4};
            SDL_RenderFillRect(app->renderer, &er);

            /* Label */
            if (zoom >= 2)
                ui_text_small(dx + 2, dy + 1, ent_labels[e->type % ENT_TYPE_COUNT]);
        }
    }

    /* ── Thin overlay scrollbars ── */
    app->scroll_y = ui_scrollbar_v(c.x + canvas_w - STYLE_SCROLLBAR_W, c.y, canvas_h,
                                    lvl->height, vis_h, app->scroll_y);
    app->scroll_x = ui_scrollbar_h(c.x, c.y + canvas_h - STYLE_SCROLLBAR_W, canvas_w,
                                    lvl->width, vis_w, app->scroll_x);

    /* Mouse interaction on canvas */
    if (ui_mouse_in_rect(ox, oy, draw_w, draw_h)) {
        int mx, my; ui_mouse_pos(&mx, &my);
        int tx = (mx - ox) / cell + app->scroll_x;
        int ty = (my - oy) / cell + app->scroll_y;

        if (tx >= 0 && tx < lvl->width && ty >= 0 && ty < lvl->height) {
            char tip[32];
            snprintf(tip, sizeof(tip), "[%d,%d] tile:%d", tx, ty, lvl->tiles[ty][tx]);
            ui_tooltip(tip);

            if ((app->sel_type == SEL_SPRITE)) {
                /* Sprite layer: click to place entity, right-click to select/delete */
                if (ui_mouse_clicked()) {
                    int hit = -1;
                    for (int i = 0; i < lvl->entity_count; i++) {
                        if (lvl->entities[i].x == tx && lvl->entities[i].y == ty) { hit = i; break; }
                    }
                    if (hit >= 0) {
                        app->sel_entity = hit;
                    } else if (lvl->entity_count < MAX_ENTITIES) {
                        lvl->entities[lvl->entity_count++] = (TilemapEntity){tx, ty, (uint8_t)app->cur_sprite, 1};
                        app->sel_entity = lvl->entity_count - 1;
                        app->modified = true;
                    }
                }
                if (ui_mouse_right_clicked()) {
                    for (int i = 0; i < lvl->entity_count; i++) {
                        if (lvl->entities[i].x == tx && lvl->entities[i].y == ty) {
                            for (int j = i; j < lvl->entity_count - 1; j++)
                                lvl->entities[j] = lvl->entities[j + 1];
                            lvl->entity_count--;
                            app->sel_entity = -1;
                            app->modified = true;
                            break;
                        }
                    }
                }
                if (app->sel_entity >= 0 && ui_mouse_down()) {
                    lvl->entities[app->sel_entity].x = tx;
                    lvl->entities[app->sel_entity].y = ty;
                    app->modified = true;
                }
            } else {
                /* Tile layer */
                if (ui_mouse_down()) {
                    lvl->tiles[ty][tx] = (uint8_t)app->cur_tset_tile;
                    app->modified = true;
                }
                if (ui_mouse_right_clicked()) {
                    app->cur_tset_tile = lvl->tiles[ty][tx];
                }
            }
        }
    }

    /* Delete selected entity with Delete key */
    if ((app->sel_type == SEL_SPRITE) && app->sel_entity >= 0 &&
        (ui_key_pressed(SDLK_DELETE) || ui_key_pressed(SDLK_BACKSPACE))) {
        for (int j = app->sel_entity; j < lvl->entity_count - 1; j++)
            lvl->entities[j] = lvl->entities[j + 1];
        lvl->entity_count--;
        app->sel_entity = -1;
        app->modified = true;
    }

    ui_panel_end();
}
