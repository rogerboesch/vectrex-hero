/*
 * tiles.h — Tile index assignments for C64
 *
 * Same layout as GBC: game tiles, destroyable, decorative, HUD, charset.
 */

#ifndef TILES_H
#define TILES_H

/* Game tiles 0-63: solid walls (collision) */
#define TILE_EMPTY        0
#define TILE_WALL         1

/* Destroyable walls 64-79 */
#define TILE_DWALL_FIRST 64
#define TILE_DWALL_LAST  79

/* Decorative tiles 80-95 (no collision) */
#define TILE_DECOR_FIRST 80
#define TILE_DECOR_LAST  95

/* HUD icons 96-101 */
#define HUD_TILE_BASE    96
#define TILE_HUD_FILL    (HUD_TILE_BASE + 0)
#define TILE_HUD_EMPTY   (HUD_TILE_BASE + 1)
#define TILE_HEART       (HUD_TILE_BASE + 2)
#define TILE_HEART_OFF   (HUD_TILE_BASE + 3)
#define TILE_DYN_ICON    (HUD_TILE_BASE + 4)
#define TILE_DYN_OFF     (HUD_TILE_BASE + 5)

/* Character set 112-150 */
#define CHAR_TILE_BASE   112
#define TILE_DIGIT_0     (CHAR_TILE_BASE + 0)
#define TILE_LETTER_A    (CHAR_TILE_BASE + 10)
#define TILE_LETTER_DASH (CHAR_TILE_BASE + 36)
#define TILE_COLON       (CHAR_TILE_BASE + 37)
#define TILE_DOT         (CHAR_TILE_BASE + 38)

/* =========================================================================
 * VIC-II hardware addresses
 * ========================================================================= */

#define VIC_BASE         0xD000
#define VIC_CTRL1        (*(uint8_t*)0xD011)  /* control register 1 */
#define VIC_CTRL2        (*(uint8_t*)0xD016)  /* control register 2 */
#define VIC_MEMPTR       (*(uint8_t*)0xD018)  /* memory pointers */
#define VIC_BORDER       (*(uint8_t*)0xD020)  /* border color */
#define VIC_BG0          (*(uint8_t*)0xD021)  /* background color 0 */

#define SCREEN_RAM       ((uint8_t*)0x0400)   /* default screen RAM */
#define COLOR_RAM        ((uint8_t*)0xD800)   /* color RAM */
#define CHARSET_RAM      ((uint8_t*)0x3000)   /* custom charset location */

/* Sprite registers */
#define SPR_ENABLE       (*(uint8_t*)0xD015)
#define SPR_MCOLOR       (*(uint8_t*)0xD01C)  /* multicolor enable */
#define SPR_XMSB         (*(uint8_t*)0xD010)  /* X MSB for all sprites */
#define SPR_PTR          ((uint8_t*)0x07F8)   /* sprite pointers */

/* Joystick port 2 */
#define JOY2             (*(uint8_t*)0xDC00)

/* Colors */
#define COLOR_BLACK       0
#define COLOR_WHITE       1
#define COLOR_RED         2
#define COLOR_CYAN        3
#define COLOR_PURPLE      4
#define COLOR_GREEN       5
#define COLOR_BLUE        6
#define COLOR_YELLOW      7
#define COLOR_ORANGE      8
#define COLOR_BROWN       9
#define COLOR_LIGHTRED   10
#define COLOR_DARKGRAY   11
#define COLOR_GRAY       12
#define COLOR_LIGHTGREEN 13
#define COLOR_LIGHTBLUE  14
#define COLOR_LIGHTGRAY  15

#endif
