//
// font.c — Custom VLC font glyph data for R.E.S.C.U.E.
//
// Each glyph: {oy, ox, count-1, dy1, dx1, dy2, dx2, ...}
// Cell size: ~6 wide x 8 tall (at font scale)
//

#include "font.h"

// Digits 0-9
static const int8_t glyph_0[] = { 0, 0, 3,  0,6, -8,0, 0,-6, 8,0 };
static const int8_t glyph_1[] = { 0, 2, 0,  -8,0 };
static const int8_t glyph_2[] = { 0, 0, 4,  0,6, -4,0, 0,-6, -4,0, 0,6 };
static const int8_t glyph_3[] = { 0, 0, 5,  0,6, -4,0, 0,-6, 0,6, -4,0, 0,-6 };
static const int8_t glyph_4[] = { 0, 0, 3,  -4,0, 0,6, 4,0, -8,0 };
static const int8_t glyph_5[] = { 0, 6, 4,  0,-6, -4,0, 0,6, -4,0, 0,-6 };
static const int8_t glyph_6[] = { 0, 6, 4,  0,-6, -8,0, 0,6, 4,0, 0,-6 };
static const int8_t glyph_7[] = { 0, 0, 1,  0,6, -8,0 };
static const int8_t glyph_8[] = { 0, 0, 5,  -8,0, 0,6, 8,0, 0,-6, -4,0, 0,6 };
static const int8_t glyph_9[] = { -4, 6, 4,  0,-6, 4,0, 0,6, -8,0, 0,-6 };

// Letters
static const int8_t glyph_A[] = { -8, 0, 4,  8,0, 0,6, -8,0, 4,0, 0,-6 };
static const int8_t glyph_B[] = { 0, 0, 5,  -8,0, 0,6, 8,0, 0,-6, -4,0, 0,6 };
static const int8_t glyph_C[] = { 0, 6, 2,  0,-6, -8,0, 0,6 };
static const int8_t glyph_D[] = { 0, 0, 3,  0,5, -8,1, 0,-6, 8,0 };
static const int8_t glyph_E[] = { 0, 6, 5,  0,-6, -4,0, 0,4, 0,-4, -4,0, 0,6 };
static const int8_t glyph_F[] = { 0, 6, 4,  0,-6, -4,0, 0,4, 0,-4, -4,0 };
static const int8_t glyph_G[] = { 0, 6, 4,  0,-6, -8,0, 0,6, 4,0, 0,-3 };
static const int8_t glyph_H[] = { 0, 0, 4,  -8,0, 4,0, 0,6, 4,0, -8,0 };
static const int8_t glyph_I[] = { 0, 1, 4,  0,4, 0,-2, -8,0, 0,-2, 0,4 };
static const int8_t glyph_J[] = { 0, 6, 1,  -8,0, 0,-6 };
static const int8_t glyph_K[] = { 0, 0, 4,  -8,0, 4,0, 4,6, -4,-6, -4,6 };
static const int8_t glyph_L[] = { 0, 0, 1,  -8,0, 0,6 };
static const int8_t glyph_M[] = { -8, 0, 3,  8,0, -4,3, 4,3, -8,0 };
static const int8_t glyph_N[] = { -8, 0, 2,  8,0, -8,6, 8,0 };
static const int8_t glyph_O[] = { 0, 0, 3,  0,6, -8,0, 0,-6, 8,0 };
static const int8_t glyph_P[] = { -8, 0, 3,  8,0, 0,6, -4,0, 0,-6 };
static const int8_t glyph_Q[] = { -8, 6, 4,  0,-6, 8,0, 0,6, -8,0, -3,3 };
static const int8_t glyph_R[] = { -8, 0, 4,  8,0, 0,6, -4,0, 0,-6, -4,6 };
static const int8_t glyph_S[] = { -8, 0, 4,  0,6, 4,0, 0,-6, 4,0, 0,6 };
static const int8_t glyph_T[] = { 0, 0, 2,  0,6, 0,-3, -8,0 };
static const int8_t glyph_U[] = { 0, 0, 2,  -8,0, 0,6, 8,0 };
static const int8_t glyph_V[] = { 0, 0, 1,  -8,3, 8,3 };
static const int8_t glyph_W[] = { 0, 0, 3,  -8,2, 4,1, -4,1, 8,2 };
static const int8_t glyph_X[] = { 0, 0, 3,  -8,6, 4,-3, -4,-3, 8,6 };
static const int8_t glyph_Y[] = { 0, 0, 3,  -4,3, 4,3, -4,-3, -4,0 };
static const int8_t glyph_Z[] = { 0, 0, 2,  0,6, -8,-6, 0,6 };

// 36-slot lookup table: 0-9 → [0..9], A-Z → [10..35]
static const int8_t * const font_table[36] = {
    glyph_0, glyph_1, glyph_2, glyph_3, glyph_4,
    glyph_5, glyph_6, glyph_7, glyph_8, glyph_9,
    glyph_A, glyph_B, glyph_C, glyph_D, glyph_E,  // A=10..E=14
    glyph_F, glyph_G, glyph_H, glyph_I, glyph_J,  // F=15..J=19
    glyph_K, glyph_L, glyph_M, glyph_N, glyph_O,  // K=20..O=24
    glyph_P, glyph_Q, glyph_R, glyph_S, glyph_T,  // P=25..T=29
    glyph_U, glyph_V, glyph_W, glyph_X, glyph_Y,  // U=30..Y=34
    glyph_Z                                          // Z=35
};

const int8_t *font_glyph(char c) {
    if (c >= '0' && c <= '9') return font_table[c - '0'];
    if (c >= 'A' && c <= 'Z') return font_table[c - 'A' + 10];
    return 0;
}
