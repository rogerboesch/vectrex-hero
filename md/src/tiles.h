/*
 * tiles.h -- Tile index assignments for Mega Drive
 * Same layout as GBC for level data compatibility.
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

/* Parallax cave tiles (Plane B) -- start after game tileset */
#define PARALLAX_TILE_BASE 160
#define PARALLAX_TILE_COUNT  4

/* VDP tile index offset (SGDK reserves tiles 0-15 for system) */
#define TILE_USER_BASE   TILE_USER_INDEX

#endif
