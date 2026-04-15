/*
 * tiles.c -- Load exported tileset and palettes into VDP
 */

#include "game.h"
#include "tiles.h"

/* Exported 4bpp tileset from export_md.py */
extern const u32 md_tileset[][8];

/* Cave-themed palettes (RGB333 format for MD) */
static const u16 pal_bg[16] = {
    0x000, 0x024, 0x046, 0x268,   /* dark cave blues */
    0x48A, 0x6AC, 0x8CE, 0xAEE,   /* lighter blues */
    0x222, 0x444, 0x666, 0x888,   /* grays */
    0xAAA, 0xCCC, 0xEEE, 0xEEE    /* light grays/white */
};

static const u16 pal_wall[16] = {
    0x000, 0x420, 0x640, 0x862,
    0xA84, 0xCA6, 0xEC8, 0xEEA,
    0x200, 0x400, 0x600, 0x800,
    0x020, 0x040, 0x060, 0xEEE
};

static const u16 pal_special[16] = {
    0x000, 0xE00, 0xEE0, 0x0EE,
    0xE0E, 0x0E0, 0x00E, 0xEEE,
    0x800, 0x880, 0x088, 0x808,
    0x080, 0x008, 0x444, 0x888
};

static const u16 pal_hud[16] = {
    0x000, 0xEEE, 0x0E0, 0xE00,
    0xEE0, 0x0EE, 0xE0E, 0x00E,
    0x888, 0x444, 0x0A0, 0xA00,
    0xAA0, 0x0AA, 0xA0A, 0x00A
};

/* =========================================================================
 * Sprite data (16x16 each = 4 tiles in VDP column order)
 * Nibble values = palette color index
 * ========================================================================= */

/* Player standing */
static const u32 spr_player[4][8] = {
    {0x00077000, 0x007FF700, 0x006F6700, 0x007FF700,
     0x000F0000, 0x00FFF000, 0x00FFF000, 0x00FFF000},
    {0x00FFF000, 0x00FF0000, 0x000FF000, 0x000F0000,
     0x000FF000, 0x000F0000, 0x00F00F00, 0x00F00F00},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Player flying */
static const u32 spr_player_fly[4][8] = {
    {0x00077000, 0x007FF700, 0x006F6700, 0x007FF700,
     0x000F0000, 0x00FFF000, 0x00FFF000, 0x00FFF000},
    {0x00FFF000, 0x000FF000, 0x000FF000, 0x000FF000,
     0x000FF000, 0x00F00F00, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Propeller frame 0 */
static const u32 spr_propeller0[4][8] = {
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x0FFFFFF0, 0x000FF000},
    {0x000FF000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Propeller frame 1 */
static const u32 spr_propeller1[4][8] = {
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x0F0000F0, 0x00F00F00, 0x000FF000},
    {0x000FF000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Bat frame 0 (wings up) */
static const u32 spr_bat0[4][8] = {
    {0x0F0000F0, 0x0F0000F0, 0x0FF00FF0, 0x0FFFFFF0,
     0x00FFFF00, 0x000FF000, 0x000FF000, 0x000F0000},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Bat frame 1 (wings down) */
static const u32 spr_bat1[4][8] = {
    {0x000FF000, 0x00FFFF00, 0x0FFFFFF0, 0x0FF00FF0,
     0x0F0000F0, 0x000FF000, 0x000FF000, 0x000F0000},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Spider */
static const u32 spr_spider[4][8] = {
    {0x000F0000, 0x000F0000, 0x000F0000, 0x00FFF000,
     0x0FFFFF00, 0x0FFFFF00, 0xF0F0F0F0, 0x0F000F00},
    {0xF00000F0, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Snake */
static const u32 spr_snake[4][8] = {
    {0x00000000, 0x00000000, 0x00000000, 0x00FFF000,
     0x0FFFFF00, 0x0FFFFFF0, 0x00FFFF00, 0x000FF000},
    {0x0000F000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Miner */
static const u32 spr_miner[4][8] = {
    {0x000F0000, 0x00FFF000, 0x0FFFFF00, 0x006F6000,
     0x00FFF000, 0x00FFF000, 0x00FFF000, 0x0FFFFF00},
    {0x00F0F000, 0x00F0F000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Dynamite */
static const u32 spr_dynamite[4][8] = {
    {0x00000000, 0x000F0000, 0x00F00000, 0x00F00000,
     0x00FFF000, 0x00FFF000, 0x00FFF000, 0x00FFF000},
    {0x00FFF000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Explosion */
static const u32 spr_explode[4][8] = {
    {0x000F0000, 0x0F000F00, 0x00F0F000, 0x000F0000,
     0xFFFFFFF0, 0x000F0000, 0x00F0F000, 0x0F000F00},
    {0x000F0000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Laser segment */
static const u32 spr_laser[4][8] = {
    {0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
};

/* Sprite VRAM tile indices */
#define SPR_TILES_PLAYER      (TILE_USER_BASE + 200)
#define SPR_TILES_PLAYER_FLY  (SPR_TILES_PLAYER + 4)
#define SPR_TILES_PROP0       (SPR_TILES_PLAYER_FLY + 4)
#define SPR_TILES_PROP1       (SPR_TILES_PROP0 + 4)
#define SPR_TILES_BAT0        (SPR_TILES_PROP1 + 4)
#define SPR_TILES_BAT1        (SPR_TILES_BAT0 + 4)
#define SPR_TILES_SPIDER      (SPR_TILES_BAT1 + 4)
#define SPR_TILES_SNAKE       (SPR_TILES_SPIDER + 4)
#define SPR_TILES_MINER       (SPR_TILES_SNAKE + 4)
#define SPR_TILES_DYNAMITE    (SPR_TILES_MINER + 4)
#define SPR_TILES_EXPLODE     (SPR_TILES_DYNAMITE + 4)
#define SPR_TILES_LASER       (SPR_TILES_EXPLODE + 4)

void tiles_init(void) {
    /* Set palettes */
    PAL_setPalette(PAL0, pal_bg, DMA);
    PAL_setPalette(PAL1, pal_wall, DMA);
    PAL_setPalette(PAL2, pal_special, DMA);
    PAL_setPalette(PAL3, pal_hud, DMA);

    /* Load exported tileset into VDP VRAM */
    VDP_loadTileData((const u32*)md_tileset, TILE_USER_BASE, 160, DMA);

    /* Load all sprite tiles into VRAM */
    VDP_loadTileData((const u32*)spr_player,      SPR_TILES_PLAYER,     4, CPU);
    VDP_loadTileData((const u32*)spr_player_fly,   SPR_TILES_PLAYER_FLY, 4, CPU);
    VDP_loadTileData((const u32*)spr_propeller0,   SPR_TILES_PROP0,      4, CPU);
    VDP_loadTileData((const u32*)spr_propeller1,   SPR_TILES_PROP1,      4, CPU);
    VDP_loadTileData((const u32*)spr_bat0,         SPR_TILES_BAT0,       4, CPU);
    VDP_loadTileData((const u32*)spr_bat1,         SPR_TILES_BAT1,       4, CPU);
    VDP_loadTileData((const u32*)spr_spider,       SPR_TILES_SPIDER,     4, CPU);
    VDP_loadTileData((const u32*)spr_snake,        SPR_TILES_SNAKE,      4, CPU);
    VDP_loadTileData((const u32*)spr_miner,        SPR_TILES_MINER,      4, CPU);
    VDP_loadTileData((const u32*)spr_dynamite,     SPR_TILES_DYNAMITE,   4, CPU);
    VDP_loadTileData((const u32*)spr_explode,      SPR_TILES_EXPLODE,    4, CPU);
    VDP_loadTileData((const u32*)spr_laser,        SPR_TILES_LASER,      4, CPU);
}
