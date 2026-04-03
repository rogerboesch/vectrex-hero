/*
 * app.h — Vectrex Studio application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "project.h"

/* Which main view is active */
typedef enum {
    VIEW_ROOM_EDITOR,
    VIEW_ROW_EDITOR,
    VIEW_SPRITE_EDITOR,
    VIEW_LEVEL_VIEWER,
    VIEW_EMULATOR
} ViewMode;

/* Room editor tools */
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
    /* Window / renderer */
    SDL_Window *window;
    SDL_Renderer *renderer;
    int win_w, win_h;

    /* View mode */
    ViewMode view;

    /* Project data */
    Project project;
    char project_path[512];
    bool modified;

    /* Selection state */
    int cur_level;
    int cur_room;
    int cur_row_type;
    int cur_sprite;
    int cur_frame;

    /* Room editor */
    RoomTool room_tool;
    int drag_idx;        /* index of dragged element, -1 = none */
    int drag_type;       /* 0=wall, 1=enemy, 2=miner, 3=player_start */
    bool dragging;

    /* Row editor */
    bool drawing_polyline;
    Polyline temp_polyline;

    /* Console */
    char console_lines[64][256];
    int console_count;
} App;

/* ── Core ─────────────────────────────────────────────────── */

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer);
void app_cleanup(App *app);
void app_draw(App *app);
void app_log(App *app, const char *fmt, ...);
void app_log_dbg(App *app, const char *fmt, ...);
void app_log_info(App *app, const char *fmt, ...);
void app_log_warn(App *app, const char *fmt, ...);
void app_log_err(App *app, const char *fmt, ...);

/* ── File I/O ─────────────────────────────────────────────── */

void app_new_project(App *app);
void app_open_project(App *app);
void app_load_project(App *app, const char *path);
void app_save_project(App *app, const char *path);
void app_save_project_as(App *app);
void app_export(App *app);

/* ── Panels ───────────────────────────────────────────────── */

void draw_level_list(App *app, int x, int y, int w, int h);
void draw_room_editor(App *app, int x, int y, int w, int h);
void draw_room_tools(App *app, int x, int y, int w, int h);
void draw_row_editor(App *app, int x, int y, int w, int h);
void draw_row_tools(App *app, int x, int y, int w, int h);
void draw_sprite_editor(App *app, int x, int y, int w, int h);
void draw_sprite_tools(App *app, int x, int y, int w, int h);
void draw_level_viewer(App *app, int x, int y, int w, int h);
void draw_emu_panel(App *app, int x, int y, int w, int h);
void draw_vcpu(App *app, int x, int y, int w, int h);
void draw_vmem(App *app, int x, int y, int w, int h);
void draw_vdisasm(App *app, int x, int y, int w, int h);
bool vectrex_emu_wants_keys(void);
void draw_console(App *app, int x, int y, int w, int h);

/* ── Coordinate helpers ───────────────────────────────────── */

/* Room coords (vx: -125..125, vy: -50..50) → pixel coords within canvas */
void room_vx_to_px(int vx, int vy, int cx, int cy, int cw, int ch, int *px, int *py);
void room_px_to_vx(int px, int py, int cx, int cy, int cw, int ch, int *vx, int *vy);
