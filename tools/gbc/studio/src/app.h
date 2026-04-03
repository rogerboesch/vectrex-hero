/*
 * app.h — GBC Studio application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "tile.h"
#include "project.h"

typedef enum {
    VIEW_TILES,
    VIEW_PALETTES,
    VIEW_ROOMS,
    VIEW_ROWS,
    VIEW_EMULATOR,
} ViewMode;

typedef enum {
    TOOL_SELECT,
    TOOL_WALL,
    TOOL_ENEMY_BAT,
    TOOL_ENEMY_SPIDER,
    TOOL_ENEMY_SNAKE,
    TOOL_MINER,
    TOOL_PLAYER_START,
} RoomTool;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int win_w, win_h;

    ViewMode view;

    /* Tiles/sprites */
    GBCTile tiles[MAX_TILES];
    int tile_count;
    int cur_tile;
    int cur_color;  /* 0-3 within current palette */

    /* Palettes */
    GBCPalette bg_pals[MAX_BG_PALS];
    GBCPalette spr_pals[MAX_SPR_PALS];
    int cur_pal_type;  /* 0=bg, 1=spr */
    int cur_pal_idx;

    /* Level data (shared with Vectrex) */
    Project level_project;
    int cur_level, cur_room, cur_row_type;
    RoomTool room_tool;
    bool drawing_polyline;
    Polyline temp_polyline;

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
void app_import_tiles_c(App *app, const char *path);

/* Panels */
void draw_tile_list(App *app, int x, int y, int w, int h);
void draw_tile_editor(App *app, int x, int y, int w, int h);
void draw_tile_tools(App *app, int x, int y, int w, int h);
void draw_palette_editor(App *app, int x, int y, int w, int h);
void draw_preview(App *app, int x, int y, int w, int h);
void draw_level_list(App *app, int x, int y, int w, int h);
void draw_room_editor(App *app, int x, int y, int w, int h);
void draw_room_tools(App *app, int x, int y, int w, int h);
void draw_row_editor(App *app, int x, int y, int w, int h);
void draw_row_tools(App *app, int x, int y, int w, int h);
void draw_gbc_emulator(App *app, int x, int y, int w, int h);

void app_load_levels(App *app, const char *path);

void room_vx_to_px(int vx, int vy, int cx, int cy, int cw, int ch, int *px, int *py);
void room_px_to_vx(int px, int py, int cx, int cy, int cw, int ch, int *vx, int *vy);
void draw_gcpu(App *app, int x, int y, int w, int h);
void draw_gmem(App *app, int x, int y, int w, int h);
void draw_gdisasm(App *app, int x, int y, int w, int h);
void draw_gbp(App *app, int x, int y, int w, int h);
bool gbc_emu_wants_keys(void);
void draw_console(App *app, int x, int y, int w, int h);
