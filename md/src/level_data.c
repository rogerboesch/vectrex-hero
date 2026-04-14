/* level_data.c -- Placeholder test level for Mega Drive */

#include "game.h"

/* Simple test level: 20x16 room with walls around the edge */
const u8 level_0_rle[] = {
    20,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    1,1, 18,0, 1,1,
    20,1,
};

const u16 level_0_row_offsets[] = {
    0, 2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86
};

const u8 level_0_entities[] = {
    5, 2, 0, 0,
};

const LevelInfo levels[NUM_LEVELS] = {
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
};
