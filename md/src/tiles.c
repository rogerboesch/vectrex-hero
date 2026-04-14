/*
 * tiles.c -- Load tileset and palettes into VDP
 *
 * Converts GBC 2bpp tile data to MD 4bpp format and uploads to VRAM.
 * Sets up 4 palettes from the project color data.
 */

#include "game.h"
#include "tiles.h"

/* Placeholder palette: dark cave colors */
static const u16 pal_bg[16] = {
    0x000, 0x222, 0x444, 0x666,   /* grays */
    0x008, 0x00A, 0x00E, 0x24E,   /* blues */
    0x800, 0xA00, 0xE00, 0xE80,   /* reds */
    0x080, 0x0A0, 0x0E0, 0xEEE    /* greens + white */
};

static const u16 pal_wall[16] = {
    0x000, 0x420, 0x640, 0x862,
    0xA84, 0xCA6, 0xEC8, 0xEEA,
    0x200, 0x400, 0x600, 0x800,
    0x020, 0x040, 0x060, 0x080
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

void tiles_init(void) {
    u32 tile4bpp[8];
    u16 i, row;

    /* Set palettes */
    VDP_setPalette(PAL0, pal_bg);
    VDP_setPalette(PAL1, pal_wall);
    VDP_setPalette(PAL2, pal_special);
    VDP_setPalette(PAL3, pal_hud);

    /* Generate placeholder tiles in VDP VRAM */
    /* Tile 0: empty (all zero) */
    memset(tile4bpp, 0, sizeof(tile4bpp));
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + 0, 1, CPU);

    /* Tile 1: solid block (all color 7) */
    for (row = 0; row < 8; row++)
        tile4bpp[row] = 0x77777777;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + 1, 1, CPU);

    /* Tile 2: checkerboard */
    for (row = 0; row < 8; row++)
        tile4bpp[row] = (row & 1) ? 0x50505050 : 0x05050505;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + 2, 1, CPU);

    /* Tile 3: border */
    tile4bpp[0] = 0x77777777;
    for (row = 1; row < 7; row++)
        tile4bpp[row] = 0x70000007;
    tile4bpp[7] = 0x77777777;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + 3, 1, CPU);

    /* Generate digit font (tiles 112-121) */
    {
        static const u8 digit_font[10][7] = {
            {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
            {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
            {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
            {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
            {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
            {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
            {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
            {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
            {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
            {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
        };
        u8 d;
        for (d = 0; d < 10; d++) {
            u8 r;
            tile4bpp[0] = 0;
            for (r = 0; r < 7; r++) {
                u8 pattern = digit_font[d][r];
                u32 pixels = 0;
                u8 bit;
                for (bit = 0; bit < 8; bit++) {
                    if (pattern & (1 << (4 - bit + 2)))  /* 5-wide font, shifted */
                        pixels |= (u32)1 << ((7 - bit) * 4);
                }
                tile4bpp[r + 1] = pixels;
            }
            VDP_loadTileData(tile4bpp, TILE_USER_BASE + 112 + d, 1, CPU);
        }
    }

    /* Generate letter font A-Z (tiles 122-147) */
    {
        static const u8 letter_font[26][7] = {
            {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
            {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
            {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
            {0x1E,0x09,0x09,0x09,0x09,0x09,0x1E},
            {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
            {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
            {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
            {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
            {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
            {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
            {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
            {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
            {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
            {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
            {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
            {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
            {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
            {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
            {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
            {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
            {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
            {0x11,0x11,0x11,0x0A,0x0A,0x04,0x04},
            {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
            {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
            {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
            {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
        };
        u8 d;
        for (d = 0; d < 26; d++) {
            u8 r;
            tile4bpp[0] = 0;
            for (r = 0; r < 7; r++) {
                u8 pattern = letter_font[d][r];
                u32 pixels = 0;
                u8 bit;
                for (bit = 0; bit < 8; bit++) {
                    if (pattern & (1 << (4 - bit + 2)))
                        pixels |= (u32)1 << ((7 - bit) * 4);
                }
                tile4bpp[r + 1] = pixels;
            }
            VDP_loadTileData(tile4bpp, TILE_USER_BASE + 122 + d, 1, CPU);
        }
    }

    /* HUD tiles */
    /* Tile 96: fuel bar filled */
    for (row = 0; row < 8; row++)
        tile4bpp[row] = (row == 0 || row == 7) ? 0x02222220 : 0x02222220;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + TILE_HUD_FILL, 1, CPU);

    /* Tile 97: fuel bar empty */
    tile4bpp[0] = 0x02222220;
    for (row = 1; row < 7; row++)
        tile4bpp[row] = 0x00000000;
    tile4bpp[7] = 0x02222220;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + TILE_HUD_EMPTY, 1, CPU);

    /* Tile 98: heart */
    tile4bpp[0] = 0x00000000;
    tile4bpp[1] = 0x03300330;
    tile4bpp[2] = 0x03333330;
    tile4bpp[3] = 0x03333330;
    tile4bpp[4] = 0x00333300;
    tile4bpp[5] = 0x00033000;
    tile4bpp[6] = 0x00003000;
    tile4bpp[7] = 0x00000000;
    VDP_loadTileData(tile4bpp, TILE_USER_BASE + TILE_HEART, 1, CPU);

    /* Parallax cave texture tiles (160-163) */
    for (i = 0; i < PARALLAX_TILE_COUNT; i++) {
        for (row = 0; row < 8; row++) {
            /* Subtle dark pattern based on tile index and row */
            u32 val = ((row + i) & 1) ? 0x01010101 : 0x10101010;
            tile4bpp[row] = val;
        }
        VDP_loadTileData(tile4bpp, TILE_USER_BASE + PARALLAX_TILE_BASE + i, 1, CPU);
    }

    /* Symbol tiles: - : . at 148-150 */
    {
        static const u8 sym_font[3][7] = {
            {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
            {0x00,0x00,0x04,0x00,0x00,0x04,0x00},
            {0x00,0x00,0x00,0x00,0x00,0x00,0x04},
        };
        u8 d;
        for (d = 0; d < 3; d++) {
            u8 r;
            tile4bpp[0] = 0;
            for (r = 0; r < 7; r++) {
                u8 pattern = sym_font[d][r];
                u32 pixels = 0;
                u8 bit;
                for (bit = 0; bit < 8; bit++) {
                    if (pattern & (1 << (4 - bit + 2)))
                        pixels |= (u32)1 << ((7 - bit) * 4);
                }
                tile4bpp[r + 1] = pixels;
            }
            VDP_loadTileData(tile4bpp, TILE_USER_BASE + 148 + d, 1, CPU);
        }
    }
}
