/*
 * sprites.h -- Apple II Hi-Res sprite definitions
 *
 * Sprites are stored in Apple II Hi-Res byte format:
 *   - 7 pixels per byte (bits 0-6), bit 7 = palette select
 *   - Monochrome white (all pixels use bit 7 = 0, white group)
 *   - Each sprite has a data array and a mask array
 *   - Mask: 1 = draw pixel, 0 = transparent
 *
 * Width is in pixels, stored width (wb) is in bytes = (w + 6) / 7.
 */

#ifndef SPRITES_H
#define SPRITES_H

#include "game.h"

typedef struct {
    uint8_t w;              /* width in pixels */
    uint8_t h;              /* height in pixels */
    uint8_t wb;             /* width in bytes = (w + 6) / 7 */
    const uint8_t *data;    /* pixel data (wb * h bytes) */
    const uint8_t *mask;    /* transparency mask (wb * h bytes) */
} Sprite;

extern const Sprite spr_player_walk_0;
extern const Sprite spr_player_walk_1;
extern const Sprite spr_player_fly_0;
extern const Sprite spr_player_fly_1;
extern const Sprite spr_player_fly_2;
extern const Sprite spr_bat_0;
extern const Sprite spr_bat_1;
extern const Sprite spr_spider_0;
extern const Sprite spr_snake_0;
extern const Sprite spr_miner_0;
extern const Sprite spr_dynamite_0;

#endif
