/*
 * app.h — GBC Studio application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "tile.h"
#include "tilemap.h"

typedef enum {
    VIEW_TILES,
    VIEW_PALETTES,
    VIEW_LEVELS,
    VIEW_EMULATOR,
} ViewMode;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int win_w, win_h;

    ViewMode view;

    /* Sprite tiles (8x16 for OAM sprites) */
    GBCTile tiles[MAX_TILES];
    int tile_count;
    int cur_tile;
    int cur_color;  /* 0-3 within current palette */

    /* Tilemap project */
    TilemapProject tmap;
    int cur_level;
    int cur_tset_tile;  /* selected tileset entry */
    int scroll_x, scroll_y;  /* tilemap canvas scroll */
    SDL_Texture *tset_texture;  /* tileset preview texture */
    bool tset_tex_dirty;

    /* Palettes */
    int cur_pal_type;  /* 0=bg, 1=spr */
    int cur_pal_idx;

    /* Project */
    char project_path[512];
    bool modified;

    /* UI */
    bool name_focus;
    char name_buf[32];

    /* Console */
    char console_lines[64][256];
    int console_count;
} App;

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer);
void app_cleanup(App *app);
void app_draw(App *app);
void app_log(App *app, const char *fmt, ...);
void app_log_dbg(App *app, const char *fmt, ...);
void app_log_info(App *app, const char *fmt, ...);
void app_log_warn(App *app, const char *fmt, ...);
void app_log_err(App *app, const char *fmt, ...);

/* File I/O */
void app_new_project(App *app);
void app_open_project(App *app);
void app_load_project(App *app, const char *path);
void app_save_project(App *app, const char *path);
void app_save_project_as(App *app);
void app_export_c(App *app);
void app_convert_levels(App *app, const char *hero_json_path);

/* Panels */
void draw_tile_list(App *app, int x, int y, int w, int h);
void draw_tile_editor(App *app, int x, int y, int w, int h);
void draw_tile_tools(App *app, int x, int y, int w, int h);
void draw_palette_editor(App *app, int x, int y, int w, int h);
void draw_level_editor(App *app, int x, int y, int w, int h);
void draw_level_tileset(App *app, int x, int y, int w, int h);
void draw_level_tools(App *app, int x, int y, int w, int h);
void draw_gbc_emulator(App *app, int x, int y, int w, int h);
void draw_gcpu(App *app, int x, int y, int w, int h);
void draw_gmem(App *app, int x, int y, int w, int h);
void draw_gdisasm(App *app, int x, int y, int w, int h);
void draw_gbp(App *app, int x, int y, int w, int h);
bool gbc_emu_wants_keys(void);
void draw_console(App *app, int x, int y, int w, int h);
