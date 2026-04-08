/*
 * tilemap.h — Tilemap data structures for GBC levels
 *
 * Each level is a scrollable tilemap up to 256x256 tiles.
 * Each tile references a tileset entry (0-255).
 * Tileset contains 256 editable 8x8 pixel tiles (GBC 2bpp format).
 */
#pragma once

#include "tile.h"
#include <string.h>

#define TMAP_MAX_W     256
#define TMAP_MAX_H     256
#define TSET_COUNT     256    /* max tiles in tileset */
#define TMAP_MAX_LEVELS 20

/* A single 8x8 tile in the tileset (reuses GBCTile but 8x8 not 8x16) */
typedef struct {
    uint8_t data[16];  /* 8x8 2bpp: 2 bytes per row */
    uint8_t palette;   /* palette index */
} TilesetEntry;

/* Get/set pixel for 8x8 tileset entry */
static inline uint8_t tset_get_pixel(const TilesetEntry *t, int x, int y) {
    uint8_t lo = t->data[y * 2], hi = t->data[y * 2 + 1];
    int bit = 7 - x;
    return ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
}

static inline void tset_set_pixel(TilesetEntry *t, int x, int y, uint8_t color) {
    int bit = 7 - x;
    uint8_t mask = ~(1 << bit);
    t->data[y * 2]     = (t->data[y * 2]     & mask) | (((color >> 0) & 1) << bit);
    t->data[y * 2 + 1] = (t->data[y * 2 + 1] & mask) | (((color >> 1) & 1) << bit);
}

/* Entity types for sprite layer */
#define ENT_PLAYER_START 0
#define ENT_BAT          1
#define ENT_SPIDER       2
#define ENT_SNAKE        3
#define ENT_MINER        4
#define ENT_TYPE_COUNT   5

#define MAX_ENTITIES     64

typedef struct {
    int x, y;       /* tile position */
    uint8_t type;   /* ENT_* */
    int8_t vx;      /* velocity for enemies */
} TilemapEntity;

/* A tilemap level */
typedef struct {
    char name[32];
    int width, height;  /* actual used size (up to 256x256) */
    uint8_t tiles[TMAP_MAX_H][TMAP_MAX_W];     /* tile indices into tileset */
    uint8_t palettes[TMAP_MAX_H][TMAP_MAX_W];   /* per-tile palette index (0-7) */
    TilemapEntity entities[MAX_ENTITIES];
    int entity_count;
} TilemapLevel;

/* The complete tileset */
typedef struct {
    TilesetEntry entries[TSET_COUNT];
    int used_count;  /* how many tiles are defined */
} Tileset;

/* A fixed 20x18 screen (title, game over, etc.) */
#define SCREEN_W       20
#define SCREEN_H       18
#define MAX_SCREENS     8

typedef struct {
    char name[32];
    uint8_t tiles[SCREEN_H][SCREEN_W];
    uint8_t palettes[SCREEN_H][SCREEN_W];
} GameScreen;

/* Full tilemap project */
typedef struct {
    Tileset tileset;
    TilemapLevel levels[TMAP_MAX_LEVELS];
    int level_count;
    GameScreen screens[MAX_SCREENS];
    int screen_count;
    GBCPalette bg_pals[MAX_BG_PALS];
    GBCPalette spr_pals[MAX_SPR_PALS];
} TilemapProject;

static inline void tilemap_project_init(TilemapProject *p) {
    memset(p, 0, sizeof(*p));
    p->level_count = 1;
    strcpy(p->levels[0].name, "Level 1");
    p->levels[0].width = 20;
    p->levels[0].height = 16;

    /* Default palettes */
    p->bg_pals[0] = (GBCPalette){{{0,0,0},{8,6,4},{16,12,8},{24,20,16}}};
    p->bg_pals[1] = (GBCPalette){{{0,0,0},{16,12,0},{24,20,4},{31,28,8}}};
    p->bg_pals[2] = (GBCPalette){{{0,0,0},{20,4,0},{31,10,0},{31,24,4}}};
    p->bg_pals[3] = (GBCPalette){{{0,0,0},{0,10,0},{0,22,0},{28,31,28}}};
    p->bg_pals[4] = (GBCPalette){{{0,0,0},{0,0,0},{2,2,4},{4,4,6}}};
    p->bg_pals[5] = (GBCPalette){{{0,0,0},{20,0,0},{31,4,4},{31,31,31}}};
    p->spr_pals[0] = (GBCPalette){{{0,0,0},{0,12,0},{0,24,4},{8,31,8}}};    /* player green */
    p->spr_pals[1] = (GBCPalette){{{0,0,0},{20,0,0},{31,8,4},{31,20,16}}};  /* bat/snake red */
    p->spr_pals[2] = (GBCPalette){{{0,0,0},{16,0,16},{26,6,26},{31,18,31}}};/* spider magenta */
    p->spr_pals[3] = (GBCPalette){{{0,0,0},{0,10,16},{0,22,28},{12,31,31}}};/* miner cyan */
    p->spr_pals[4] = (GBCPalette){{{0,0,0},{16,16,0},{28,28,4},{31,31,20}}};/* laser yellow */

    /* Default tileset: tile 0 = empty (black), tile 1 = solid rock */
    /* Tile 0: all black */
    memset(&p->tileset.entries[0], 0, sizeof(TilesetEntry));
    /* Tile 1: solid fill (color 2 = checkerboard) */
    for (int y = 0; y < 8; y++) {
        uint8_t lo = 0, hi = 0;
        for (int x = 0; x < 8; x++) {
            uint8_t c = ((x ^ y) & 1) ? 2 : 1;
            int bit = 7 - x;
            lo |= ((c & 1) << bit);
            hi |= (((c >> 1) & 1) << bit);
        }
        p->tileset.entries[1].data[y * 2] = lo;
        p->tileset.entries[1].data[y * 2 + 1] = hi;
    }
    /* Tile 2: border (color 3 = bright) */
    for (int y = 0; y < 8; y++) {
        uint8_t lo = 0, hi = 0;
        for (int x = 0; x < 8; x++) {
            bool edge = (y <= 1 || y >= 6 || x <= 1 || x >= 6);
            uint8_t c = edge ? 3 : (((x ^ y) & 1) ? 2 : 1);
            int bit = 7 - x;
            lo |= ((c & 1) << bit);
            hi |= (((c >> 1) & 1) << bit);
        }
        p->tileset.entries[2].data[y * 2] = lo;
        p->tileset.entries[2].data[y * 2 + 1] = hi;
    }
    p->tileset.used_count = 3;
}
