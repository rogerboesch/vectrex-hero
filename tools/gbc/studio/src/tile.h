/*
 * tile.h — GBC tile/sprite data structures
 *
 * GBC tiles are 8x8 or 8x16, 2 bits per pixel (4 colors per palette).
 * Format: 2 bytes per row — lo plane, hi plane.
 * Pixel = (hi_bit << 1) | lo_bit → color 0-3.
 *
 * Sprites in H.E.R.O. are 8x16 (32 bytes each).
 */
#pragma once

#include <stdint.h>
#include <string.h>

#define TILE_W         8
#define TILE_H        16    /* 8x16 sprite mode */
#define TILE_BYTES    32    /* 2 bytes/row * 16 rows */
#define MAX_TILES     32
#define COLORS_PER_PAL 4
#define MAX_BG_PALS    6
#define MAX_SPR_PALS   5

typedef struct {
    char name[32];
    uint8_t data[TILE_BYTES];  /* GBC 2bpp format: [row*2]=lo, [row*2+1]=hi */
    int palette;               /* palette index */
} GBCTile;

typedef struct {
    uint8_t r, g, b;  /* 5-bit each (0-31) */
} RGB5;

typedef struct {
    RGB5 colors[COLORS_PER_PAL];
} GBCPalette;

/* Get pixel color (0-3) at position */
static inline uint8_t tile_get_pixel(const GBCTile *t, int x, int y) {
    uint8_t lo = t->data[y * 2];
    uint8_t hi = t->data[y * 2 + 1];
    int bit = 7 - x;
    return ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
}

/* Set pixel color (0-3) at position */
static inline void tile_set_pixel(GBCTile *t, int x, int y, uint8_t color) {
    int bit = 7 - x;
    uint8_t mask = ~(1 << bit);
    t->data[y * 2]     = (t->data[y * 2]     & mask) | (((color >> 0) & 1) << bit);
    t->data[y * 2 + 1] = (t->data[y * 2 + 1] & mask) | (((color >> 1) & 1) << bit);
}

/* Clear tile */
static inline void tile_clear(GBCTile *t) {
    memset(t->data, 0, TILE_BYTES);
}

/* Flip tile horizontally */
static inline void tile_flip_h(GBCTile *t) {
    for (int y = 0; y < TILE_H; y++) {
        uint8_t lo = 0, hi = 0;
        for (int x = 0; x < TILE_W; x++) {
            uint8_t c = tile_get_pixel(t, x, y);
            int bit = x;  /* reversed */
            lo |= ((c >> 0) & 1) << bit;
            hi |= ((c >> 1) & 1) << bit;
        }
        t->data[y * 2] = lo;
        t->data[y * 2 + 1] = hi;
    }
}

/* Flip tile vertically */
static inline void tile_flip_v(GBCTile *t) {
    for (int y = 0; y < TILE_H / 2; y++) {
        int y2 = TILE_H - 1 - y;
        uint8_t tmp_lo = t->data[y * 2];
        uint8_t tmp_hi = t->data[y * 2 + 1];
        t->data[y * 2]      = t->data[y2 * 2];
        t->data[y * 2 + 1]  = t->data[y2 * 2 + 1];
        t->data[y2 * 2]     = tmp_lo;
        t->data[y2 * 2 + 1] = tmp_hi;
    }
}

/* Convert RGB5 to SDL-style 8-bit RGB */
static inline void rgb5_to_rgb8(RGB5 c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (c.r * 255) / 31;
    *g = (c.g * 255) / 31;
    *b = (c.b * 255) / 31;
}
