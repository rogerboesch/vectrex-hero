/*
 * project.h — Vectrex H.E.R.O. project data structures
 *
 * Mirrors the JSON format used by the Python level_editor.py.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────── */

#define VX_MIN        -128
#define VX_MAX         127
#define MAX_LEVELS      20
#define MAX_ROOMS       16
#define MAX_ROW_TYPES   32
#define MAX_SPRITES     32
#define MAX_WALLS       16
#define MAX_ENEMIES      8
#define MAX_POLYLINES   16
#define MAX_POINTS     128
#define MAX_FRAMES      16

/* Room boundaries */
#define ROOM_LEFT     -125
#define ROOM_RIGHT     125
#define ROOM_TOP        50
#define ROOM_BOTTOM    -50

/* Row boundaries (local coords) */
#define ROW_LEFT      -125
#define ROW_RIGHT      125
#define ROW_BOTTOM       0
#define ROW_TOP         30

/* Row boundary Y positions in room coords */
#define ROW_BOUNDARY_TOP   20
#define ROW_BOUNDARY_MID  -10

/* Enemy types */
#define ENEMY_BAT       0
#define ENEMY_SPIDER    1
#define ENEMY_SNAKE     2

/* ── Data types ───────────────────────────────────────────── */

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point points[MAX_POINTS];
    int count;
} Polyline;

typedef struct {
    char name[64];
    Polyline cave_lines[MAX_POLYLINES];
    int cave_line_count;
} RowType;

typedef struct {
    int y, x, h, w;
    bool destroyable;
} Wall;

typedef struct {
    int x, y, vx;
    int type;  /* ENEMY_BAT, ENEMY_SPIDER, ENEMY_SNAKE */
} Enemy;

typedef struct {
    int number;
    int exit_left, exit_right, exit_top, exit_bottom; /* -1 = none */
    int rows[3];          /* [top, mid, bottom] indices into row_types */
    Wall walls[MAX_WALLS];
    int wall_count;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
    Point miner;
    bool has_miner;
    Point player_start;
    bool has_player_start;
    bool has_lava;
} Room;

typedef struct {
    char name[64];
    Room rooms[MAX_ROOMS];
    int room_count;
} Level;

typedef struct {
    Point points[MAX_POINTS];
    int count;
} SpriteFrame;

typedef struct {
    char name[64];
    SpriteFrame frames[MAX_FRAMES];
    int frame_count;
} VecSprite;

typedef struct {
    RowType row_types[MAX_ROW_TYPES];
    int row_type_count;
    Level levels[MAX_LEVELS];
    int level_count;
    VecSprite sprites[MAX_SPRITES];
    int sprite_count;
} Project;

/* ── Helpers ──────────────────────────────────────────────── */

static inline void project_init(Project *p) {
    memset(p, 0, sizeof(*p));
    /* Initialize 32 row types */
    p->row_type_count = 32;
    for (int i = 0; i < 32; i++) {
        snprintf(p->row_types[i].name, 64, "Row Type %d", i);
    }
    /* One default level with one room */
    p->level_count = 1;
    strcpy(p->levels[0].name, "Level 1");
    p->levels[0].room_count = 1;
    p->levels[0].rooms[0].number = 1;
    p->levels[0].rooms[0].exit_left = -1;
    p->levels[0].rooms[0].exit_right = -1;
    p->levels[0].rooms[0].exit_top = -1;
    p->levels[0].rooms[0].exit_bottom = -1;
    /* One default sprite */
    p->sprite_count = 1;
    strcpy(p->sprites[0].name, "sprite");
    p->sprites[0].frame_count = 1;
}

static inline int clamp_i(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
