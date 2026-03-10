//
// levels.h — Level data arrays and cave geometry constants
//

#ifndef LEVELS_H
#define LEVELS_H

#include "hero.h"

// =========================================================================
// Level data
// =========================================================================

// Collision walls for level 1 (horizontal surfaces only)
// Shaft walls handled by code
#define L1_WALL_COUNT 3
static const int8_t l1_walls[] = {
    // y, x, h, w  (center-y, left-x, half-height, width)
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,  // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT, // right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT // floor
};

// Level 1 enemies
#define L1_ENEMY_COUNT 1
static const int8_t l1_enemies[] = {
    // x, y, vx
    0, 0, 1     // bat in middle of shaft
};

// Level 2 layout: same shape but with a destroyable wall across shaft
#define L2_WALL_COUNT 4
static const int8_t l2_walls[] = {
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,    // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT,// right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT, // floor
    12, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT               // destroyable wall
};

#define L2_ENEMY_COUNT 2
static const int8_t l2_enemies[] = {
    -10, 0, 1,     // bat below destroyable wall
    0, 50, -1      // bat above in entry area
};

#endif
