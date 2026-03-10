//
// sprites.h — VLC sprite arrays (static data)
//

#ifndef SPRITES_H
#define SPRITES_H

// Bat - wings spread
static int8_t bat_frame1[] = {
    5,
    0,   -6,    // left wing out
    4,   3,     // left wing up
    0,   6,     // body across
    4,   3,     // right wing up
    0,   -6     // right wing back
};

// Bat - wings down
static int8_t bat_frame2[] = {
    5,
    0,   -6,    // left wing out
    -3,  3,     // left wing down
    0,   6,     // body across
    -3,  3,     // right wing down
    0,   -6     // right wing back
};

// Miner (rescue target)
static int8_t miner_shape[] = {
    5,
    5,   0,     // head up
    -5,  0,     // back down
    -6,  0,     // body
    0,   -3,    // left leg
    0,   6      // right leg
};

// Dynamite stick
static int8_t dynamite_shape[] = {
    3,
    0,   3,     // top
    -6,  0,     // side
    0,   -3     // bottom
};

#endif
