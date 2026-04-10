/*
 * app.h — GBC Workbench application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "tile.h"
#include "tilemap.h"

typedef enum {
    VIEW_LEVELS,
    VIEW_EDITOR,
    VIEW_SPRITES,
    VIEW_SCREENS,
    VIEW_EMULATOR,
} ViewMode;

typedef enum {
    SEL_TILE,     /* a tileset entry is selected */
    SEL_SPRITE,   /* a sprite is selected */
} SelectionType;

typedef enum {
    CLIP_NONE = 0,
    CLIP_TILE,
    CLIP_SPRITE,
} ClipboardType;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int win_w, win_h;

    ViewMode view;

    /* Sprite tiles (8x16 for OAM sprites) */
    GBCTile sprites[MAX_TILES];
    int sprite_count;
    int cur_color;  /* 0-3 within current palette */

    /* Tilemap project */
    TilemapProject tmap;
    int cur_level;
    int cur_tset_tile;     /* selected tileset entry index */
    int cur_sprite;        /* selected sprite index */
    SelectionType sel_type;/* what's currently selected */
    int scroll_x, scroll_y;
    int cur_palette;       /* current BG palette for painting (0-7) */
    int sel_entity;        /* selected entity index in level, -1=none */
    int marker_row;        /* row marker for shift operations, -1=none */
    int cur_screen;        /* selected screen index for screen editor */
    bool dmg_preview;      /* show tiles in DMG green palette */

    /* Project */
    char project_path[512];
    char build_dir[512];       /* Directory containing the Makefile */
    char rom_name[64];         /* ROM output filename (e.g. "rescue.gbc") */
    bool modified;

    /* Clipboard */
    ClipboardType clip_type;
    TilesetEntry clip_tile;
    GBCTile clip_sprite;

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
void app_test_level(App *app);
/* Panels */
void draw_asset_list(App *app, int x, int y, int w, int h);  /* tiles + palettes */
void draw_sprite_list(App *app, int x, int y, int w, int h); /* sprites */
void draw_level_editor(App *app, int x, int y, int w, int h);
void draw_level_tools(App *app, int x, int y, int w, int h);
void draw_pixel_editor(App *app, int x, int y, int w, int h);
void draw_editor_tools(App *app, int x, int y, int w, int h);
void draw_screen_editor(App *app, int x, int y, int w, int h);
void draw_gbc_emulator(App *app, int x, int y, int w, int h);
void draw_gcpu(App *app, int x, int y, int w, int h);
void draw_gmem(App *app, int x, int y, int w, int h);
void draw_gdisasm(App *app, int x, int y, int w, int h);
void draw_gbp(App *app, int x, int y, int w, int h);
bool gbc_emu_wants_keys(void);
void draw_console(App *app, int x, int y, int w, int h);
