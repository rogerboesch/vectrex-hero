//
// sprites.c — Cave Diver VLC sprite data (placeholder)
//

#include "sprites.h"

// Placeholder sprites — simple geometric shapes
// Will be replaced with real VLC art via the sprite editor

// Player facing right — simple triangle
int8_t player_right[] = {
    4, 6, -6, -4,       // hw, hh, oy, ox
    4,                   // count-1
    6, -4,
    0, 8,
    -6, -4,
    0, 0
};

// Player facing left — mirrored
int8_t player_left[] = {
    4, 6, -6, 4,
    4,
    6, 4,
    0, -8,
    -6, 4,
    0, 0
};

// Player swimming
int8_t player_swim[] = {
    6, 3, -3, -6,
    4,
    3, 12,
    -3, 0,
    -3, -12,
    3, 0
};

// Fish
int8_t fish_right[] = {
    4, 3, -3, -4,
    4,
    3, 8,
    -3, -2,
    3, -2,
    -3, -4
};

// Jellyfish
int8_t jellyfish[] = {
    3, 4, -4, -3,
    5,
    4, 0,
    0, 6,
    -4, 0,
    -2, -2,
    2, -2,
    -2, 2
};

// Eel
int8_t eel_right[] = {
    6, 2, -2, -6,
    4,
    2, 4,
    -2, 4,
    2, 4,
    -2, -12
};

// Diver (rescue target)
int8_t diver_sprite[] = {
    3, 6, -6, -3,
    5,
    3, 0,
    -1, 3,
    -3, -3,
    3, -3,
    -3, 3,
    1, 0
};
