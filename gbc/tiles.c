//
// tiles.c — Tile generation, sprite data, palette setup
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// CGB Palettes (RGB555)
// =========================================================================

static const uint16_t bg_palettes[] = {
    // PAL_BG_CAVE (0) — brown rock
    RGB(0,0,0), RGB(8,6,4), RGB(16,12,8), RGB(24,20,16),
    // PAL_BG_DWALL (1) — yellow/gold destroyable
    RGB(0,0,0), RGB(16,12,0), RGB(24,20,4), RGB(31,28,8),
    // PAL_BG_LAVA (2) — red/orange
    RGB(0,0,0), RGB(20,4,0), RGB(31,10,0), RGB(31,24,4),
    // PAL_BG_HUD (3) — green/white
    RGB(0,0,0), RGB(0,10,0), RGB(0,22,0), RGB(28,31,28),
    // PAL_BG_EMPTY (4) — all black
    RGB(0,0,0), RGB(0,0,0), RGB(2,2,4), RGB(4,4,6),
    // PAL_BG_HUD2 (5) — red/white for hearts
    RGB(0,0,0), RGB(20,0,0), RGB(31,4,4), RGB(31,31,31),
};

static const uint16_t spr_palettes[] = {
    // PAL_SP_PLAYER (0) — green
    RGB(0,0,0), RGB(0,12,0), RGB(0,24,4), RGB(8,31,8),
    // PAL_SP_BAT (1) — red (also snake)
    RGB(0,0,0), RGB(20,0,0), RGB(31,8,4), RGB(31,20,16),
    // PAL_SP_SPIDER (2) — magenta
    RGB(0,0,0), RGB(16,0,16), RGB(26,6,26), RGB(31,18,31),
    // PAL_SP_MINER (3) — cyan
    RGB(0,0,0), RGB(0,10,16), RGB(0,22,28), RGB(12,31,31),
    // PAL_SP_LASER (4) — yellow/white
    RGB(0,0,0), RGB(16,16,0), RGB(28,28,4), RGB(31,31,20),
};

// =========================================================================
// Generate auto-tiled wall variants (16 tiles based on neighbor mask)
// =========================================================================
//
// mask bits: 0=up wall, 1=right wall, 2=down wall, 3=left wall
// Exposed edge (neighbor is empty) gets bright pixels (color 3).
// Interior gets textured fill (colors 1-2).
//
static void gen_wall_tiles(void) {
    uint8_t tile[16];
    uint8_t mask, row, col, pixel, lo, hi;
    uint8_t top_open, right_open, bottom_open, left_open, is_edge;

    for (mask = 0; mask < 16; mask++) {
        top_open    = !(mask & 0x01);
        right_open  = !(mask & 0x02);
        bottom_open = !(mask & 0x04);
        left_open   = !(mask & 0x08);

        for (row = 0; row < 8; row++) {
            lo = 0; hi = 0;
            for (col = 0; col < 8; col++) {
                is_edge = 0;
                if (row <= 1 && top_open) is_edge = 1;
                if (row >= 6 && bottom_open) is_edge = 1;
                if (col <= 1 && left_open) is_edge = 1;
                if (col >= 6 && right_open) is_edge = 1;

                // Corner highlight where two edges meet
                if (row <= 1 && col <= 1 && top_open && left_open) pixel = 3;
                else if (row <= 1 && col >= 6 && top_open && right_open) pixel = 3;
                else if (row >= 6 && col <= 1 && bottom_open && left_open) pixel = 3;
                else if (row >= 6 && col >= 6 && bottom_open && right_open) pixel = 3;
                else if (is_edge) pixel = 3;
                else if (((row ^ col) & 1) == 0) pixel = 2;
                else pixel = 1;

                lo |= ((pixel & 1) << (7 - col));
                hi |= (((pixel >> 1) & 1) << (7 - col));
            }
            tile[row * 2] = lo;
            tile[row * 2 + 1] = hi;
        }
        set_bkg_data(TILE_WALL + mask, 1, tile);
    }
}

// =========================================================================
// Generate special tiles
// =========================================================================

static void gen_empty_tile(void) {
    uint8_t tile[16];
    memset(tile, 0, 16);
    set_bkg_data(TILE_EMPTY, 1, tile);
}

static void gen_destroyable_wall(void) {
    // Solid bright tile with cross-hatch pattern
    uint8_t tile[16];
    uint8_t row, col, lo, hi, pixel;
    for (row = 0; row < 8; row++) {
        lo = 0; hi = 0;
        for (col = 0; col < 8; col++) {
            if (row == 0 || row == 7 || col == 0 || col == 7)
                pixel = 3;  // bright border
            else if ((row + col) & 1)
                pixel = 2;  // cross-hatch
            else
                pixel = 3;
            lo |= ((pixel & 1) << (7 - col));
            hi |= (((pixel >> 1) & 1) << (7 - col));
        }
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_DWALL, 1, tile);

    // Edge variant (slightly different pattern)
    for (row = 0; row < 8; row++) {
        lo = 0; hi = 0;
        for (col = 0; col < 8; col++) {
            if (row == 0 || row == 7 || col == 0 || col == 7)
                pixel = 3;
            else
                pixel = 2;
            lo |= ((pixel & 1) << (7 - col));
            hi |= (((pixel >> 1) & 1) << (7 - col));
        }
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_DWALL_EDGE, 1, tile);
}

static void gen_lava_tiles(void) {
    // Lava frame 0: wavy top
    const uint8_t lava0[16] = {
        0x00, 0x00,  // ........
        0x49, 0x49,  // .1..1..1   wave peaks (color 3)
        0xB6, 0xB6,  // 1.11.11.   wave body
        0xFF, 0xFF,  // 11111111
        0xFF, 0xFF,  // 11111111
        0xDB, 0xDB,  // 11.11.11   texture
        0xFF, 0xFF,  // 11111111
        0xFF, 0xFF,  // 11111111
    };
    set_bkg_data(TILE_LAVA0, 1, lava0);

    // Lava frame 1: shifted wave
    const uint8_t lava1[16] = {
        0x00, 0x00,
        0x92, 0x92,  // 1..1..1.
        0x6D, 0x6D,  // .11.11.1
        0xFF, 0xFF,
        0xFF, 0xFF,
        0xB6, 0xB6,
        0xFF, 0xFF,
        0xFF, 0xFF,
    };
    set_bkg_data(TILE_LAVA1, 1, lava1);
}

static void gen_hud_tiles(void) {
    uint8_t tile[16];
    uint8_t row, lo, hi;

    // Fuel bar filled — solid green bar
    for (row = 0; row < 8; row++) {
        if (row == 0 || row == 7) { lo = 0x7E; hi = 0x7E; }  // bright border
        else { lo = 0x00; hi = 0x7E; }  // medium fill
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_HUD_FILL, 1, tile);

    // Fuel bar empty — dark outline
    for (row = 0; row < 8; row++) {
        if (row == 0 || row == 7) { lo = 0x7E; hi = 0x00; }  // dark border
        else { lo = 0x00; hi = 0x00; }  // empty
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_HUD_EMPTY, 1, tile);

    // Heart (life) — color 3
    //  .XX.XX..
    //  XXXXXXX.  (actually 6 wide)
    //  XXXXXXX.
    //  .XXXXX..
    //  ..XXX...
    //  ...X....
    const uint8_t heart[16] = {
        0x00, 0x00,
        0x6C, 0x6C,  // .XX.XX..
        0xFE, 0xFE,  // XXXXXXX.
        0xFE, 0xFE,  // XXXXXXX.
        0x7C, 0x7C,  // .XXXXX..
        0x38, 0x38,  // ..XXX...
        0x10, 0x10,  // ...X....
        0x00, 0x00,
    };
    set_bkg_data(TILE_HEART, 1, heart);

    // Heart off — dim outline, color 1
    const uint8_t heart_off[16] = {
        0x00, 0x00,
        0x6C, 0x00,  // .11.11..
        0x92, 0x00,  // 1..1..1.
        0x82, 0x00,  // 1.....1.
        0x44, 0x00,  // .1...1..
        0x28, 0x00,  // ..1.1...
        0x10, 0x00,  // ...1....
        0x00, 0x00,
    };
    set_bkg_data(TILE_HEART_OFF, 1, heart_off);

    // Dynamite icon — color 3
    const uint8_t dyn_icon[16] = {
        0x10, 0x10,  // ...X....  fuse
        0x08, 0x08,  // ....X...
        0x3C, 0x3C,  // ..XXXX..
        0x3C, 0x3C,  // ..XXXX..
        0x3C, 0x3C,  // ..XXXX..
        0x3C, 0x3C,  // ..XXXX..
        0x3C, 0x3C,  // ..XXXX..
        0x00, 0x00,
    };
    set_bkg_data(TILE_DYN_ICON, 1, dyn_icon);

    // Dynamite off — dim
    const uint8_t dyn_off[16] = {
        0x00, 0x00,
        0x00, 0x00,
        0x3C, 0x00,  // ..1111..
        0x24, 0x00,  // ..1..1..
        0x24, 0x00,  // ..1..1..
        0x24, 0x00,  // ..1..1..
        0x3C, 0x00,  // ..1111..
        0x00, 0x00,
    };
    set_bkg_data(TILE_DYN_OFF, 1, dyn_off);
}

// =========================================================================
// Digit font — 5x7 bitmap per digit, generate 8x8 tiles
// =========================================================================

static const uint8_t digit_font[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 9
};

// Letters: L V S C R E - : H O G B A T N U D F I P .
static const uint8_t letter_font[][7] = {
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L  (37)
    {0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x04}, // V  (38)
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S  (39)
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C  (40)
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R  (41)
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E  (42)
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -  (43)
    {0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00}, // :  (44)
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H  (45)
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O  (46)
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G  (47)
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B  (48)
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A  (49)
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T  (50)
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}, // N  (51)
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U  (52)
    {0x1E, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1E}, // D  (53)
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F  (54)
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I  (55)
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P  (56)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}, // .  (57)
};

static void gen_font_tiles(void) {
    uint8_t tile[16];
    uint8_t d, row, pattern;

    // Digits 0-9
    for (d = 0; d < 10; d++) {
        tile[0] = 0; tile[1] = 0;  // top padding
        for (row = 0; row < 7; row++) {
            pattern = digit_font[d][row] << 2;
            tile[(row + 1) * 2]     = pattern;  // lo=hi for color 3
            tile[(row + 1) * 2 + 1] = pattern;
        }
        set_bkg_data(TILE_DIGIT_0 + d, 1, tile);
    }

    // Letters L,V,S,C,R,E,-,:,H,O,G,B,A,T,N,U,D,F,I,P,.
    for (d = 0; d < 21; d++) {
        tile[0] = 0; tile[1] = 0;
        for (row = 0; row < 7; row++) {
            pattern = letter_font[d][row] << 2;
            tile[(row + 1) * 2]     = pattern;
            tile[(row + 1) * 2 + 1] = pattern;
        }
        set_bkg_data(TILE_LETTER_L + d, 1, tile);
    }
}

// =========================================================================
// Sprite pixel data (8x16 mode = 32 bytes each)
// =========================================================================

// Player right — standing
static const uint8_t spr_player_r[] = {
    0x00, 0x00,  // ........
    0x38, 0x38,  // ..XXX...  helmet (color 3)
    0x44, 0x7C,  // .X.X.X.  head (mix 2+3)
    0x00, 0x6C,  // .XX.XX..  eyes (color 2)
    0x44, 0x7C,  // .XXXXX.  chin
    0x00, 0x10,  // ...X....  neck
    0x7C, 0x7C,  // .XXXXX.  collar (color 3)
    0x00, 0x7C,  // .XXXXX.  body (color 2)
    0x00, 0x7C,  // .XXXXX.  body
    0x7C, 0x00,  // .XXXXX.  belt (color 1)
    0x00, 0x30,  // ..XX....  legs
    0x00, 0x30,  // ..XX....
    0x00, 0x18,  // ...XX...
    0x00, 0x18,  // ...XX...
    0x00, 0x24,  // ..X..X..  feet
    0x66, 0x00,  // .XX..XX.  shoes (color 1)
};

// Player walk frame
static const uint8_t spr_player_walk[] = {
    0x00, 0x00,
    0x38, 0x38,  // helmet
    0x44, 0x7C,  // head
    0x00, 0x6C,  // eyes
    0x44, 0x7C,  // chin
    0x00, 0x10,  // neck
    0x7C, 0x7C,  // collar
    0x00, 0x7C,  // body
    0x00, 0x7C,  // body
    0x7C, 0x00,  // belt
    0x00, 0x20,  // ..X.....  left leg forward
    0x00, 0x20,  // ..X.....
    0x00, 0x10,  // ...X....
    0x00, 0x08,  // ....X...  right leg back
    0x00, 0x44,  // .X...X..  feet spread
    0x44, 0x00,  // .X...X..  shoes
};

// Player flying
static const uint8_t spr_player_fly[] = {
    0x00, 0x00,
    0x38, 0x38,  // helmet
    0x44, 0x7C,  // head
    0x00, 0x6C,  // eyes
    0x44, 0x7C,  // chin
    0x00, 0x10,  // neck
    0x7C, 0x7C,  // collar
    0x00, 0x7C,  // body
    0x00, 0x7C,  // body
    0x7C, 0x00,  // belt
    0x00, 0x18,  // ...XX...  legs together
    0x00, 0x18,  // ...XX...
    0x00, 0x18,  // ...XX...
    0x00, 0x18,  // ...XX...  dangling
    0x00, 0x24,  // ..X..X..  feet
    0x00, 0x00,
};

// Propeller frame 0 (blades horizontal, positioned above player)
static const uint8_t spr_prop0[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0xFE, 0xFE,  // XXXXXXX.  blades horizontal
    0x10, 0x10,  // ...X....  shaft
    0x10, 0x10,  // ...X....
    0x00, 0x00,
};

// Propeller frame 1 (blades angled)
static const uint8_t spr_prop1[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x42, 0x42,  // .X....X.  blades angled
    0x24, 0x24,  // ..X..X..
    0x18, 0x18,  // ...XX...  center
    0x10, 0x10,  // ...X....  shaft
    0x00, 0x00,
};

// Bat frame 0 (wings up)
static const uint8_t spr_bat0[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x82, 0x82,  // X.....X.  wing tips up
    0x44, 0xC6,  // .X...X..  wings mid (mix)
    0x7C, 0xFE,  // .XXXXX..  wings out
    0x00, 0x7C,  // .XXXXX..  body
    0x00, 0x38,  // ..XXX...  body
    0x38, 0x38,  // ..XXX...  eyes (bright)
    0x00, 0x10,  // ...X....  mouth
    0x10, 0x00,  // ...X....  fang (dark)
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Bat frame 1 (wings down)
static const uint8_t spr_bat1[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x38,  // ..XXX...  body top
    0x00, 0x7C,  // .XXXXX..  body
    0x7C, 0xFE,  // .XXXXX..  wings out
    0x44, 0xC6,  // .X...X..  wings mid
    0x82, 0x82,  // X.....X.  wing tips down
    0x38, 0x38,  // ..XXX...  eyes
    0x00, 0x10,  // ...X....  mouth
    0x10, 0x00,  // ...X....  fang
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Spider
static const uint8_t spr_spider[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x10, 0x10,  // ...X....  thread
    0x10, 0x10,  // ...X....
    0x10, 0x10,  // ...X....
    0x28, 0x38,  // ..XXX...  head (mix)
    0x44, 0x7C,  // .XXXXX.  body
    0x44, 0x7C,  // .XXXXX.  body
    0xAA, 0xEE,  // X.X.X.X  legs out
    0x44, 0x44,  // .X...X..  lower legs
    0x82, 0x82,  // X.....X.  feet
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Snake right
static const uint8_t spr_snake_r[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x38, 0x38,  // ..XXX...  head
    0x54, 0x7C,  // .XXXXX.  head+eyes
    0x00, 0xFE,  // XXXXXXX.  body wide
    0x00, 0x7C,  // .XXXXX..  body
    0x00, 0x38,  // ..XXX...  tail
    0x00, 0x10,  // ...X....  tail tip
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Miner sitting
static const uint8_t spr_miner[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x10, 0x10,  // ...X....  lamp
    0x38, 0x38,  // ..XXX...  helmet
    0x44, 0x7C,  // .XXXXX.  head
    0x00, 0x6C,  // .XX.XX..  face
    0x00, 0x38,  // ..XXX...  chin
    0x00, 0x7C,  // .XXXXX..  body
    0x00, 0x7C,  // .XXXXX..  body
    0x7C, 0x00,  // .XXXXX.  belt (dark)
    0x00, 0x7E,  // .XXXXXX. legs out
    0x00, 0x42,  // .X....X. knees
    0x00, 0x42,  // .X....X. feet
    0x00, 0x00,
    0x00, 0x00,
};

// Dynamite
static const uint8_t spr_dynamite[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x08, 0x08,  // ....X...  fuse spark
    0x10, 0x10,  // ...X....  fuse
    0x10, 0x10,
    0x38, 0x38,  // ..XXX...  dynamite body
    0x38, 0x38,
    0x38, 0x38,
    0x38, 0x38,
    0x38, 0x38,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Laser segment (horizontal bar)
static const uint8_t spr_laser[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0xFF, 0xFF,  // XXXXXXXX  beam top
    0xFF, 0xFF,  // XXXXXXXX  beam
    0xFF, 0xFF,  // XXXXXXXX  beam
    0xFF, 0xFF,  // XXXXXXXX  beam bottom
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// Explosion
static const uint8_t spr_explode[] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x10, 0x10,  // ...X....
    0x44, 0x44,  // .X...X..
    0x28, 0x28,  // ..X.X...
    0x10, 0x10,  // ...X....
    0xFE, 0xFE,  // XXXXXXX.  center burst
    0x10, 0x10,  // ...X....
    0x28, 0x28,  // ..X.X...
    0x44, 0x44,  // .X...X..
    0x10, 0x10,  // ...X....
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

// =========================================================================
// Initialize all tiles, sprites, and palettes
// =========================================================================

void tiles_init(void) {
    // Set CGB palettes
    set_bkg_palette(0, 6, bg_palettes);
    set_sprite_palette(0, 5, spr_palettes);

    // Generate background tiles
    gen_empty_tile();
    gen_wall_tiles();
    gen_destroyable_wall();
    gen_lava_tiles();
    gen_hud_tiles();
    gen_font_tiles();

    // Load sprite tiles
    set_sprite_data(SPR_PLAYER_R,    2, spr_player_r);
    set_sprite_data(SPR_PLAYER_WALK, 2, spr_player_walk);
    set_sprite_data(SPR_PLAYER_FLY,  2, spr_player_fly);
    set_sprite_data(SPR_PROP0,       2, spr_prop0);
    set_sprite_data(SPR_PROP1,       2, spr_prop1);
    set_sprite_data(SPR_BAT0,        2, spr_bat0);
    set_sprite_data(SPR_BAT1,        2, spr_bat1);
    set_sprite_data(SPR_SPIDER,      2, spr_spider);
    set_sprite_data(SPR_SNAKE_R,     2, spr_snake_r);
    set_sprite_data(SPR_MINER,       2, spr_miner);
    set_sprite_data(SPR_DYNAMITE,    2, spr_dynamite);
    set_sprite_data(SPR_LASER,       2, spr_laser);
    set_sprite_data(SPR_EXPLODE,     2, spr_explode);

    // Enable 8x16 sprite mode
    SPRITES_8x16;
}
