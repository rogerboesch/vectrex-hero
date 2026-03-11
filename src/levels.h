//
// levels.h — Level data arrays and cave geometry constants
//

#ifndef LEVELS_H
#define LEVELS_H

#include "hero.h"

// =========================================================================
// Level 1, Room 1
// =========================================================================

#define L1R1_WALL_COUNT 3
static const int8_t l1r1_walls[] = {
    // y, x, h, w  (center-y, left-x, half-height, width)
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,  // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT, // right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT // floor
};

#define L1R1_ENEMY_COUNT 1
static const int8_t l1r1_enemies[] = {
    // x, y, vx
    0, 0, 1     // bat in middle of shaft
};

#define L1R1_START_X  (CAVE_LEFT + 15)
#define L1R1_START_Y  (CAVE_TOP - 2)
#define L1R1_MINER_X  0
#define L1R1_MINER_Y  (CAVE_FLOOR + 8)

// Room lookup tables for Level 1
static const int8_t * const l1_room_walls[] = { l1r1_walls };
static const uint8_t l1_room_wall_counts[] = { L1R1_WALL_COUNT };
static const int8_t * const l1_room_enemies[] = { l1r1_enemies };
static const uint8_t l1_room_enemy_counts[] = { L1R1_ENEMY_COUNT };
static const int8_t l1_room_starts[] = {
    L1R1_START_X, L1R1_START_Y
};
static const int8_t l1_room_miners[] = {
    L1R1_MINER_X, L1R1_MINER_Y
};
static const uint8_t l1_room_exits[] = {
    NONE, NONE, NONE, NONE   // room 0: no exits
};
static const int8_t l1_room_bounds[] = {
    CAVE_LEFT, CAVE_RIGHT, CAVE_TOP, CAVE_FLOOR   // room 0
};
static const char l1_name[] = "LEVEL 1";
#define L1_NAME_X -31  // -(7 * 9 / 2)

// =========================================================================
// Level 2, Room 1
// =========================================================================

#define L2R1_WALL_COUNT 4
static const int8_t l2r1_walls[] = {
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,    // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT,// right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT, // floor
    12, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT               // destroyable wall
};

#define L2R1_ENEMY_COUNT 2
static const int8_t l2r1_enemies[] = {
    -10, 0, 1,     // bat below destroyable wall
    0, 50, -1      // bat above in entry area
};

#define L2R1_START_X  (CAVE_LEFT + 15)
#define L2R1_START_Y  (CAVE_TOP - 2)
#define L2R1_MINER_X  0
#define L2R1_MINER_Y  (CAVE_FLOOR + 8)

// Room lookup tables for Level 2
static const int8_t * const l2_room_walls[] = { l2r1_walls };
static const uint8_t l2_room_wall_counts[] = { L2R1_WALL_COUNT };
static const int8_t * const l2_room_enemies[] = { l2r1_enemies };
static const uint8_t l2_room_enemy_counts[] = { L2R1_ENEMY_COUNT };
static const int8_t l2_room_starts[] = {
    L2R1_START_X, L2R1_START_Y
};
static const int8_t l2_room_miners[] = {
    L2R1_MINER_X, L2R1_MINER_Y
};
static const uint8_t l2_room_exits[] = {
    NONE, NONE, NONE, NONE   // room 0: no exits
};
static const int8_t l2_room_bounds[] = {
    CAVE_LEFT, CAVE_RIGHT, CAVE_TOP, CAVE_FLOOR   // room 0
};
static const char l2_name[] = "LEVEL 2";
#define L2_NAME_X -31  // -(7 * 9 / 2)

#endif
