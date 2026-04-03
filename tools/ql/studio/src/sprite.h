/*
 * sprite.h — Sprite data for QL Mode 8 (pure C)
 */
#pragma once

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SPRITE_W 64
#define MAX_SPRITE_H 64
#define MAX_SPRITES 256

/* QL Mode 8 colors (RGBA for display) */
static const uint32_t QL_COLORS_RGBA[8] = {
    0x000000FF, /* 0: black (transparent) */
    0xFF0000FF, /* 1: red */
    0x0000FFFF, /* 2: blue */
    0xFF00FFFF, /* 3: magenta */
    0x00FF00FF, /* 4: green */
    0x00FFFFFF, /* 5: cyan */
    0xFFFF00FF, /* 6: yellow */
    0xFFFFFFFF, /* 7: white */
};

static const char *QL_COLOR_NAMES[8] = {
    "transparent", "red", "blue", "magenta",
    "green", "cyan", "yellow", "white"
};

/* SDL_Color versions for UI rendering */
#include <SDL.h>
static const SDL_Color QL_SDL_COLORS[8] = {
    {  0,   0,   0, 255},
    {255,   0,   0, 255},
    {  0,   0, 255, 255},
    {255,   0, 255, 255},
    {  0, 255,   0, 255},
    {  0, 255, 255, 255},
    {255, 255,   0, 255},
    {255, 255, 255, 255},
};

typedef struct {
    char name[64];
    int width;      /* must be even */
    int height;
    uint8_t pixels[MAX_SPRITE_H][MAX_SPRITE_W]; /* color index 0-7 */
} Sprite;

static inline void sprite_init(Sprite *s, const char *name, int w, int h) {
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->width = w;
    s->height = h;
}

static inline void sprite_clear(Sprite *s) {
    memset(s->pixels, 0, sizeof(s->pixels));
}

static inline void sprite_flip_h(Sprite *s) {
    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width / 2; x++) {
            uint8_t tmp = s->pixels[y][x];
            s->pixels[y][x] = s->pixels[y][s->width - 1 - x];
            s->pixels[y][s->width - 1 - x] = tmp;
        }
    }
}

static inline void sprite_flip_v(Sprite *s) {
    for (int y = 0; y < s->height / 2; y++) {
        uint8_t tmp[MAX_SPRITE_W];
        memcpy(tmp, s->pixels[y], s->width);
        memcpy(s->pixels[y], s->pixels[s->height - 1 - y], s->width);
        memcpy(s->pixels[s->height - 1 - y], tmp, s->width);
    }
}

static inline void sprite_move_up(Sprite *s) {
    uint8_t tmp[MAX_SPRITE_W];
    memcpy(tmp, s->pixels[0], s->width);
    for (int y = 0; y < s->height - 1; y++)
        memcpy(s->pixels[y], s->pixels[y + 1], s->width);
    memcpy(s->pixels[s->height - 1], tmp, s->width);
}

static inline void sprite_move_down(Sprite *s) {
    uint8_t tmp[MAX_SPRITE_W];
    memcpy(tmp, s->pixels[s->height - 1], s->width);
    for (int y = s->height - 1; y > 0; y--)
        memcpy(s->pixels[y], s->pixels[y - 1], s->width);
    memcpy(s->pixels[0], tmp, s->width);
}

static inline void sprite_move_left(Sprite *s) {
    for (int y = 0; y < s->height; y++) {
        uint8_t first = s->pixels[y][0];
        memmove(&s->pixels[y][0], &s->pixels[y][1], s->width - 1);
        s->pixels[y][s->width - 1] = first;
    }
}

static inline void sprite_move_right(Sprite *s) {
    for (int y = 0; y < s->height; y++) {
        uint8_t last = s->pixels[y][s->width - 1];
        memmove(&s->pixels[y][1], &s->pixels[y][0], s->width - 1);
        s->pixels[y][0] = last;
    }
}

static inline void sprite_resize(Sprite *s, int new_w, int new_h) {
    if (new_w < 2) new_w = 2;
    if (new_w > MAX_SPRITE_W) new_w = MAX_SPRITE_W;
    if (new_w & 1) new_w++;
    if (new_h < 1) new_h = 1;
    if (new_h > MAX_SPRITE_H) new_h = MAX_SPRITE_H;

    /* Zero out new areas */
    if (new_w > s->width) {
        for (int y = 0; y < s->height; y++)
            memset(&s->pixels[y][s->width], 0, new_w - s->width);
    }
    if (new_h > s->height) {
        for (int y = s->height; y < new_h; y++)
            memset(s->pixels[y], 0, new_w);
    }
    s->width = new_w;
    s->height = new_h;
}
