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

/* Player sprite (16x16, 4bpp = 4 tiles of 8x8)
 * Tile order: top-left, bottom-left, top-right, bottom-right (VDP column order) */
static const u32 player_sprite[4][8] = {
    /* Tile 0: top-left */
    {0x00077000,  /* ..OOO... */
     0x007FF700,  /* .OXXO... */
     0x006F6700,  /* .OX.XO.. */
     0x007FF700,  /* .OXXXO.. */
     0x000F0000,  /* ...X.... */
     0x00FFF000,  /* .XXXO... */
     0x00FFF000,  /* .XXXO... */
     0x00FFF000}, /* .XXXO... */
    /* Tile 1: bottom-left */
    {0x00FFF000,  /* .XXXO... */
     0x00FF0000,  /* .XXO.... */
     0x000F0000,  /* ...X.... */
     0x000FF000,  /* ..XX.... */
     0x000F0000,  /* ...X.... */
     0x000FF000,  /* ..XX.... */
     0x00F00F00,  /* .X...X.. */
     0x00F00F00}, /* .X...X.. */
    /* Tile 2: top-right (empty) */
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    /* Tile 3: bottom-right (empty) */
    {0x00000000, 0x00000000, 0x00000000, 0x00000000,
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
};

#define SPRITE_TILE_BASE (TILE_USER_BASE + 200)

void tiles_init(void) {
    /* Set palettes */
    PAL_setPalette(PAL0, pal_bg, DMA);
    PAL_setPalette(PAL1, pal_wall, DMA);
    PAL_setPalette(PAL2, pal_special, DMA);
    PAL_setPalette(PAL3, pal_hud, DMA);

    /* Load exported tileset into VDP VRAM */
    VDP_loadTileData((const u32*)md_tileset, TILE_USER_BASE, 160, DMA);

    /* Load player sprite tiles */
    VDP_loadTileData((const u32*)player_sprite, SPRITE_TILE_BASE, 4, CPU);

    /* Init sprite engine */
    SPR_init();
}
