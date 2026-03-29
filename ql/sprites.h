/*
 * sprites.h — Sprite bitmap data for QL Mode 8
 *
 * Each sprite: width (pixels, even), height (pixels), pixel data.
 * Mode 8 format: 2 pixels per byte (high nibble = left, low nibble = right).
 * Color 0 = transparent.
 */

#ifndef SPRITES_H
#define SPRITES_H

#include "game.h"

typedef struct {
    uint8_t w;          /* width in pixels (must be even) */
    uint8_t h;          /* height in pixels */
    const uint8_t *data; /* w/2 * h bytes */
} Sprite;

extern const Sprite spr_player_r;
extern const Sprite spr_player_walk;
extern const Sprite spr_player_fly;
extern const Sprite spr_bat0;
extern const Sprite spr_bat1;
extern const Sprite spr_spider;
extern const Sprite spr_snake_r;
extern const Sprite spr_miner;
extern const Sprite spr_dynamite;
extern const Sprite spr_laser;
extern const Sprite spr_explode;

#endif
