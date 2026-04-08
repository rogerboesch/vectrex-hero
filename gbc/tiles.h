//
// tiles.h — Tile and sprite index assignments, palette data
//

#ifndef TILES_H
#define TILES_H

// =========================================================================
// Background tile indices
// =========================================================================

// Tiles 0..188 are the Workbench tileset (loaded from tileset_export.c)
#define TILE_EMPTY       0
#define TILE_WALL        1     // main wall tile (for title screen decorations)
#define TILE_DWALL       17    // destroyable wall (collision check)
#define TILE_DWALL_EDGE  18    // destroyable wall edge
#define TILE_LAVA0       19    // lava frame 0 (collision check)
#define TILE_LAVA1       20    // lava frame 1

// HUD and font tiles appended to Workbench tileset (indices 104-140)
#define HUD_TILE_BASE    104U
#define TILE_HUD_FILL    (HUD_TILE_BASE + 0)   // fuel bar filled
#define TILE_HUD_EMPTY   (HUD_TILE_BASE + 1)   // fuel bar empty
#define TILE_HEART       (HUD_TILE_BASE + 2)   // life heart
#define TILE_HEART_OFF   (HUD_TILE_BASE + 3)   // lost life
#define TILE_DYN_ICON    (HUD_TILE_BASE + 4)   // dynamite icon
#define TILE_DYN_OFF     (HUD_TILE_BASE + 5)   // spent dynamite
#define TILE_DIGIT_0     (HUD_TILE_BASE + 6)   // 6..15: digits 0-9
#define TILE_LETTER_L    (HUD_TILE_BASE + 16)
#define TILE_LETTER_V    (HUD_TILE_BASE + 17)
#define TILE_LETTER_S    (HUD_TILE_BASE + 18)
#define TILE_LETTER_C    (HUD_TILE_BASE + 19)
#define TILE_LETTER_R    (HUD_TILE_BASE + 20)
#define TILE_LETTER_E    (HUD_TILE_BASE + 21)
#define TILE_LETTER_DASH (HUD_TILE_BASE + 22)
#define TILE_COLON       (HUD_TILE_BASE + 23)
#define TILE_LETTER_H    (HUD_TILE_BASE + 24)
#define TILE_LETTER_O    (HUD_TILE_BASE + 25)
#define TILE_LETTER_G    (HUD_TILE_BASE + 26)
#define TILE_LETTER_B    (HUD_TILE_BASE + 27)
#define TILE_LETTER_A    (HUD_TILE_BASE + 28)
#define TILE_LETTER_T    (HUD_TILE_BASE + 29)
#define TILE_LETTER_N    (HUD_TILE_BASE + 30)
#define TILE_LETTER_U    (HUD_TILE_BASE + 31)
#define TILE_LETTER_D    (HUD_TILE_BASE + 32)
#define TILE_LETTER_F    (HUD_TILE_BASE + 33)
#define TILE_LETTER_I    (HUD_TILE_BASE + 34)
#define TILE_LETTER_P    (HUD_TILE_BASE + 35)
#define TILE_DOT         (HUD_TILE_BASE + 36)
// tiles up to 225 used (189 tileset + 37 HUD/font)

// =========================================================================
// Sprite tile indices (8x16 mode: each sprite = 2 consecutive tiles)
// =========================================================================

#define SPR_PLAYER_R     0    // player right (tiles 0-1)
#define SPR_PLAYER_WALK  2    // player walk frame (tiles 2-3)
#define SPR_PLAYER_FLY   4    // player flying (tiles 4-5)
#define SPR_PROP0        6    // propeller frame 0 (tiles 6-7)
#define SPR_PROP1        8    // propeller frame 1 (tiles 8-9)
#define SPR_BAT0         10   // bat frame 0 (tiles 10-11)
#define SPR_BAT1         12   // bat frame 1 (tiles 12-13)
#define SPR_SPIDER       14   // spider (tiles 14-15)
#define SPR_SNAKE_R      16   // snake right (tiles 16-17)
#define SPR_MINER        18   // miner sitting (tiles 18-19)
#define SPR_DYNAMITE     20   // dynamite (tiles 20-21)
#define SPR_LASER        22   // laser segment (tiles 22-23)
#define SPR_EXPLODE      24   // explosion (tiles 24-25)

// =========================================================================
// OAM sprite slots
// =========================================================================

#define OAM_PLAYER       0
#define OAM_PROP         1
#define OAM_ENEMY0       2
#define OAM_ENEMY1       3
#define OAM_ENEMY2       4
#define OAM_MINER        5
#define OAM_DYNAMITE     6
#define OAM_LASER0       7
#define OAM_LASER1       8
#define OAM_LASER2       9
#define OAM_COUNT        10

// =========================================================================
// CGB palette indices
// =========================================================================

// Background palettes
#define PAL_BG_CAVE      0    // brown rock
#define PAL_BG_DWALL     1    // yellow destroyable
#define PAL_BG_LAVA      2    // red lava
#define PAL_BG_HUD       3    // green/white HUD
#define PAL_BG_EMPTY     4    // black
#define PAL_BG_HUD2      5    // red/white for hearts

// Sprite palettes
#define PAL_SP_PLAYER    0    // green
#define PAL_SP_BAT       1    // red
#define PAL_SP_SPIDER    2    // magenta
#define PAL_SP_MINER     3    // cyan
#define PAL_SP_LASER     4    // yellow
#define PAL_SP_SNAKE     1    // red (shared with bat)

// RGB(r,g,b) is provided by gb/cgb.h

#endif
