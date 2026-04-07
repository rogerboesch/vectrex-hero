//
// tiles.c — Load exported tileset, sprites, and palettes from GBC Workbench
//           Generate HUD and font tiles in code (not part of Workbench tileset)
//

#include "game.h"
#include "tiles.h"
#include "tileset_export.h"
#include "sprites_export.h"
#include "palettes_export.h"

// =========================================================================
// HUD tiles (generated at runtime — not in Workbench tileset)
// =========================================================================

static void gen_hud_tiles(void) {
    uint8_t tile[16];
    uint8_t row, lo, hi;

    // Fuel bar filled — solid green bar
    for (row = 0; row < 8; row++) {
        if (row == 0 || row == 7) { lo = 0x7E; hi = 0x7E; }
        else { lo = 0x00; hi = 0x7E; }
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_HUD_FILL, 1, tile);

    // Fuel bar empty — dark outline
    for (row = 0; row < 8; row++) {
        if (row == 0 || row == 7) { lo = 0x7E; hi = 0x00; }
        else { lo = 0x00; hi = 0x00; }
        tile[row * 2] = lo;
        tile[row * 2 + 1] = hi;
    }
    set_bkg_data(TILE_HUD_EMPTY, 1, tile);

    // Heart (life)
    {
        const uint8_t heart[16] = {
            0x00, 0x00,
            0x6C, 0x6C,
            0xFE, 0xFE,
            0xFE, 0xFE,
            0x7C, 0x7C,
            0x38, 0x38,
            0x10, 0x10,
            0x00, 0x00,
        };
        set_bkg_data(TILE_HEART, 1, heart);
    }

    // Heart off — dim outline
    {
        const uint8_t heart_off[16] = {
            0x00, 0x00,
            0x6C, 0x00,
            0x92, 0x00,
            0x82, 0x00,
            0x44, 0x00,
            0x28, 0x00,
            0x10, 0x00,
            0x00, 0x00,
        };
        set_bkg_data(TILE_HEART_OFF, 1, heart_off);
    }

    // Dynamite icon
    {
        const uint8_t dyn_icon[16] = {
            0x10, 0x10,
            0x08, 0x08,
            0x3C, 0x3C,
            0x3C, 0x3C,
            0x3C, 0x3C,
            0x3C, 0x3C,
            0x3C, 0x3C,
            0x00, 0x00,
        };
        set_bkg_data(TILE_DYN_ICON, 1, dyn_icon);
    }

    // Dynamite off — dim
    {
        const uint8_t dyn_off[16] = {
            0x00, 0x00,
            0x00, 0x00,
            0x3C, 0x00,
            0x24, 0x00,
            0x24, 0x00,
            0x24, 0x00,
            0x3C, 0x00,
            0x00, 0x00,
        };
        set_bkg_data(TILE_DYN_OFF, 1, dyn_off);
    }
}

// =========================================================================
// Font tiles: digits 0-9 and letters (generated at runtime)
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

static const uint8_t letter_font[][7] = {
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x04}, // V
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00}, // :
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}, // N
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x1E, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}, // .
};

static void gen_font_tiles(void) {
    uint8_t tile[16];
    uint8_t d, row, pattern;

    for (d = 0; d < 10; d++) {
        tile[0] = 0; tile[1] = 0;
        for (row = 0; row < 7; row++) {
            pattern = digit_font[d][row] << 2;
            tile[(row + 1) * 2]     = pattern;
            tile[(row + 1) * 2 + 1] = pattern;
        }
        set_bkg_data(TILE_DIGIT_0 + d, 1, tile);
    }

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
// Initialize all tiles, sprites, and palettes
// =========================================================================

void tiles_init(void) {
    uint8_t saved_bank = _current_bank;

    // Load palettes (in bank 0)
    set_bkg_palette(0, EXPORT_BG_PAL_COUNT, exported_bg_palettes);
    set_sprite_palette(0, EXPORT_SPR_PAL_COUNT, exported_spr_palettes);

    // Load tileset from Workbench export (in bank 0)
    // Load in two batches to avoid any uint8_t issues
    set_bkg_data(0, 128, (const uint8_t *)tileset_data);
    set_bkg_data(128, TILESET_COUNT - 128, (const uint8_t *)tileset_data[128]);

    // Switch to bank 3 for sprite data
    SWITCH_ROM(5);

    set_sprite_data(SPR_BAT0,        2, spr_bat_0);
    set_sprite_data(SPR_BAT1,        2, spr_bat_1);
    set_sprite_data(SPR_DYNAMITE,    2, spr_dynamite_0);
    set_sprite_data(SPR_EXPLODE,     2, spr_explode_0);
    set_sprite_data(SPR_MINER,       2, spr_miner_0);
    set_sprite_data(SPR_PLAYER_FLY,  2, spr_player_fly_0);
    set_sprite_data(SPR_PROP0,       2, spr_player_fly_1);
    set_sprite_data(SPR_PROP1,       2, spr_player_fly_2);
    set_sprite_data(SPR_PLAYER_R,    2, spr_player_walk_0);
    set_sprite_data(SPR_PLAYER_WALK, 2, spr_player_walk_1);
    set_sprite_data(SPR_SNAKE_R,     2, spr_snake_0);
    set_sprite_data(SPR_SPIDER,      2, spr_spider_0);

    SWITCH_ROM(saved_bank);

    // Overwrite HUD + font tile slots with generated tiles (in bank 0)
    gen_hud_tiles();
    gen_font_tiles();

    // Enable 8x16 sprite mode
    SPRITES_8x16;
}
