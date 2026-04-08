//
// tiles.c — Load exported tileset, sprites, and palettes from GBC Workbench
//

#include "game.h"
#include "tiles.h"
#include "tileset_export.h"
#include "sprites_export.h"
#include "palettes_export.h"

// =========================================================================
// Initialize all tiles, sprites, and palettes
// =========================================================================

void tiles_init(void) {
    // Load palettes
    set_bkg_palette(0, EXPORT_BG_PAL_COUNT, exported_bg_palettes);
    set_sprite_palette(0, EXPORT_SPR_PAL_COUNT, exported_spr_palettes);

    // Load tileset (includes game tiles + HUD/font tiles)
    set_bkg_data(0, TILESET_COUNT, (const uint8_t *)tileset_data);

    // Load sprites
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

    // Spider thread: thin vertical line (1px wide, color 1)
    {
        static const uint8_t thread_tile[32] = {
            0x08,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
            0x08,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
            0x08,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
            0x08,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
        };
        set_sprite_data(SPR_THREAD, 2, thread_tile);
    }

    // Enable 8x16 sprite mode
    SPRITES_8x16;
}
