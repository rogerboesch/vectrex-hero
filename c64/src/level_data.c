/*
 * level_data.c — Placeholder level data for C64
 * TODO: Generate from GBC Workbench export
 */

#include "game.h"

/* Simple test level: 20x16 room with walls around the edge */
static const uint8_t level_0_rle[] = {
    20,1,  /* row 0: all wall */
    1,1, 18,0, 1,1,  /* rows 1-14: wall + empty + wall */
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
    20,1,  /* row 15: all wall */
};

static const uint16_t level_0_row_offsets[] = {
    0, 2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86
};

static const uint8_t level_0_entities[] = {
    5, 2, 0, 0,  /* player start at (5,2) */
};

const LevelInfo levels[NUM_LEVELS] = {
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    /* Remaining levels point to same placeholder */
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
    {20, 16, level_0_rle, level_0_row_offsets, 1, level_0_entities},
};
