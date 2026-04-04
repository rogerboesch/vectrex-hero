//
// render.c — Scrolling tilemap renderer, window HUD, sprites
//

#include "game.h"
#include "tiles.h"

// No window layer — HUD is baked into background tilemap rows 0-1

// =========================================================================
// VRAM shadow maps (what's currently in the 32x32 VRAM tile maps)
// =========================================================================

static uint8_t tilemap[VRAM_MAP_H][VRAM_MAP_W];
static uint8_t attrmap[VRAM_MAP_H][VRAM_MAP_W];

// HUD tilemap (window layer, 20x2)
static uint8_t hud_tiles[2][20];
static uint8_t hud_attrs[2][20];

// =========================================================================
// Tile-to-palette mapping
// =========================================================================

static uint8_t tile_palette(uint8_t tile_idx) {
    if (tile_idx == 0) return PAL_BG_EMPTY;
    if (tile_idx >= 1 && tile_idx <= 16) return PAL_BG_CAVE;
    if (tile_idx == TILE_DWALL || tile_idx == TILE_DWALL_EDGE) return PAL_BG_DWALL;
    if (tile_idx == TILE_LAVA0 || tile_idx == TILE_LAVA1) return PAL_BG_LAVA;
    return PAL_BG_CAVE;
}

// =========================================================================
// Fill a column in the VRAM map from decode cache
// =========================================================================

static void stream_column(uint8_t level_col) {
    uint8_t vram_col = level_col & (VRAM_MAP_W - 1);
    uint8_t start_row = cam_ty;
    uint8_t buf_tile[VRAM_MAP_H];
    uint8_t buf_attr[VRAM_MAP_H];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PLAY_ROWS + 2 && start_row + i < level_h; i++) {
        uint8_t lr = start_row + i;
        uint8_t vr = lr & (VRAM_MAP_H - 1);
        uint8_t t = tile_at(level_col, lr);
        tilemap[vr][vram_col] = t;
        attrmap[vr][vram_col] = tile_palette(t);
        buf_tile[count] = t;
        buf_attr[count] = tile_palette(t);
        count++;
    }

    // Upload column to VRAM tile by tile
    for (uint8_t i = 0; i < count; i++) {
        uint8_t lr = start_row + i;
        uint8_t vr = lr & (VRAM_MAP_H - 1);
        VBK_REG = 0;
        set_bkg_tiles(vram_col, vr, 1, 1, &buf_tile[i]);
        VBK_REG = 1;
        set_bkg_tiles(vram_col, vr, 1, 1, &buf_attr[i]);
    }
    VBK_REG = 0;
}

// =========================================================================
// Fill a row in the VRAM map from decode cache
// =========================================================================

static void stream_row(uint8_t level_row) {
    uint8_t vram_row = level_row & (VRAM_MAP_H - 1);
    uint8_t start_col = cam_tx;
    uint8_t buf_tile[VRAM_MAP_W];
    uint8_t buf_attr[VRAM_MAP_W];
    uint8_t count = 0;

    for (uint8_t i = 0; i < PLAY_COLS + 2 && start_col + i < level_w; i++) {
        uint8_t lc = start_col + i;
        uint8_t vc = lc & (VRAM_MAP_W - 1);
        uint8_t t = tile_at(lc, level_row);
        tilemap[vram_row][vc] = t;
        attrmap[vram_row][vc] = tile_palette(t);
        buf_tile[count] = t;
        buf_attr[count] = tile_palette(t);
        count++;
    }

    for (uint8_t i = 0; i < count; i++) {
        uint8_t lc = start_col + i;
        uint8_t vc = lc & (VRAM_MAP_W - 1);
        VBK_REG = 0;
        set_bkg_tiles(vc, vram_row, 1, 1, &buf_tile[i]);
        VBK_REG = 1;
        set_bkg_tiles(vc, vram_row, 1, 1, &buf_attr[i]);
    }
    VBK_REG = 0;
}

// =========================================================================
// Initialize level rendering: fill entire visible VRAM area
// =========================================================================

void render_init_level(void) {
    // Set camera to center on player
    cam_x = player_px - (SCREEN_W / 2);
    cam_y = player_py - (PLAY_H / 2);

    // Clamp camera
    int16_t max_cx = (int16_t)level_w * 8 - SCREEN_W;
    int16_t max_cy = (int16_t)level_h * 8 - PLAY_H;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > max_cx) cam_x = max_cx;
    if (cam_y > max_cy) cam_y = max_cy;

    cam_tx = (uint8_t)(cam_x >> 3);
    cam_ty = (uint8_t)(cam_y >> 3);

    // Simple approach: rows 0-1 = HUD, rows 2-17 = level tiles
    // Fill 18 rows (20 cols) into a flat buffer and upload
    {
        uint8_t row_tiles[20];
        uint8_t row_attrs[20];
        uint8_t r, c;

        // Rows 2-17: level tiles
        for (r = 0; r < 16; r++) {
            uint8_t ly = cam_ty + r;
            for (c = 0; c < 20; c++) {
                uint8_t lx = cam_tx + c;
                uint8_t t;
                if (lx < level_w && ly < level_h)
                    t = tile_at(lx, ly);
                else
                    t = 1;
                row_tiles[c] = t;
                row_attrs[c] = tile_palette(t);
            }
            VBK_REG = 0;
            set_bkg_tiles(0, r + HUD_ROWS, 20, 1, row_tiles);
            VBK_REG = 1;
            set_bkg_tiles(0, r + HUD_ROWS, 20, 1, row_attrs);
        }
        VBK_REG = 0;
    }

    SCX_REG = 0;
    SCY_REG = 0;

    // Build HUD into background rows 0-1
    render_update_hud();
}

// =========================================================================
// Camera update: track player, stream tiles
// =========================================================================

void render_update_camera(void) {
    // Track player
    int16_t target_x = player_px - (SCREEN_W / 2);
    int16_t target_y = player_py - (PLAY_H / 2);

    int16_t max_cx = (int16_t)level_w * 8 - SCREEN_W;
    int16_t max_cy = (int16_t)level_h * 8 - PLAY_H;
    if (max_cx < 0) max_cx = 0;
    if (max_cy < 0) max_cy = 0;
    if (target_x < 0) target_x = 0;
    if (target_y < 0) target_y = 0;
    if (target_x > max_cx) target_x = max_cx;
    if (target_y > max_cy) target_y = max_cy;

    cam_x = target_x;
    cam_y = target_y;

    uint8_t new_tx = (uint8_t)(cam_x >> 3);
    uint8_t new_ty = (uint8_t)(cam_y >> 3);

    // If camera tile position changed, re-render the playfield
    if (new_tx != cam_tx || new_ty != cam_ty) {
        cam_tx = new_tx;
        cam_ty = new_ty;
        uint8_t row_tiles[20];
        uint8_t row_attrs[20];
        uint8_t r, c;
        for (r = 0; r < 16; r++) {
            uint8_t ly = cam_ty + r;
            for (c = 0; c < 20; c++) {
                uint8_t lx = cam_tx + c;
                uint8_t t;
                if (lx < level_w && ly < level_h)
                    t = tile_at(lx, ly);
                else
                    t = 1;
                row_tiles[c] = t;
                row_attrs[c] = tile_palette(t);
            }
            VBK_REG = 0;
            set_bkg_tiles(0, r + HUD_ROWS, 20, 1, row_tiles);
            VBK_REG = 1;
            set_bkg_tiles(0, r + HUD_ROWS, 20, 1, row_attrs);
        }
        VBK_REG = 0;
    }

    // Sub-tile scroll: use SCX/SCY for pixel-level offset within tiles
    SCX_REG = cam_x & 7;
    SCY_REG = cam_y & 7;
}

// =========================================================================
// Clear a tile (for destroyed walls)
// =========================================================================

void render_clear_tile(uint8_t tx, uint8_t ty) {
    uint8_t vx = tx & (VRAM_MAP_W - 1);
    uint8_t vy = ty & (VRAM_MAP_H - 1);
    tilemap[vy][vx] = 0;
    attrmap[vy][vx] = PAL_BG_EMPTY;
    VBK_REG = 0;
    set_bkg_tiles(vx, vy, 1, 1, &tilemap[vy][vx]);
    VBK_REG = 1;
    set_bkg_tiles(vx, vy, 1, 1, &attrmap[vy][vx]);
    VBK_REG = 0;
}

// =========================================================================
// HUD (window layer)
// =========================================================================

void render_update_hud(void) {
    uint8_t c;

    for (c = 0; c < 20; c++) {
        hud_tiles[0][c] = TILE_EMPTY;
        hud_tiles[1][c] = TILE_EMPTY;
        hud_attrs[0][c] = PAL_BG_HUD;
        hud_attrs[1][c] = PAL_BG_HUD;
    }

    // Level number
    hud_tiles[0][0] = TILE_LETTER_L;
    hud_tiles[0][1] = TILE_DIGIT_0 + ((current_level + 1) / 10);
    hud_tiles[0][2] = TILE_DIGIT_0 + ((current_level + 1) % 10);

    // Hearts
    hud_tiles[0][4] = (player_lives >= 1) ? TILE_HEART : TILE_HEART_OFF;
    hud_tiles[0][5] = (player_lives >= 2) ? TILE_HEART : TILE_HEART_OFF;
    hud_tiles[0][6] = (player_lives >= 3) ? TILE_HEART : TILE_HEART_OFF;
    hud_attrs[0][4] = PAL_BG_HUD2;
    hud_attrs[0][5] = PAL_BG_HUD2;
    hud_attrs[0][6] = PAL_BG_HUD2;

    // Dynamite icons
    hud_tiles[0][8]  = (player_dynamite >= 1) ? TILE_DYN_ICON : TILE_DYN_OFF;
    hud_tiles[0][9]  = (player_dynamite >= 2) ? TILE_DYN_ICON : TILE_DYN_OFF;
    hud_tiles[0][10] = (player_dynamite >= 3) ? TILE_DYN_ICON : TILE_DYN_OFF;
    hud_attrs[0][8]  = PAL_BG_DWALL;
    hud_attrs[0][9]  = PAL_BG_DWALL;
    hud_attrs[0][10] = PAL_BG_DWALL;

    // Score
    {
        int16_t s = score;
        uint8_t d;
        hud_tiles[0][13] = TILE_LETTER_S;
        hud_tiles[0][14] = TILE_LETTER_C;
        hud_tiles[0][15] = TILE_COLON;
        for (d = 0; d < 4; d++) {
            hud_tiles[0][19 - d] = TILE_DIGIT_0 + (s % 10);
            s /= 10;
        }
    }

    // Fuel bar
    {
        uint8_t filled = (uint8_t)((uint16_t)player_fuel * 20 / START_FUEL);
        for (c = 0; c < 20; c++) {
            hud_tiles[1][c] = (c < filled) ? TILE_HUD_FILL : TILE_HUD_EMPTY;
            hud_attrs[1][c] = PAL_BG_HUD;
        }
    }

    // Upload to background rows 0-1
    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 1, &hud_tiles[0][0]);
    set_bkg_tiles(0, 1, 20, 1, &hud_tiles[1][0]);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, 20, 1, &hud_attrs[0][0]);
    set_bkg_tiles(0, 1, 20, 1, &hud_attrs[1][0]);
    VBK_REG = 0;
}

// =========================================================================
// Sprite update
// =========================================================================

static uint8_t sprite_visible(int16_t wx, int16_t wy) {
    int16_t sx = wx - cam_x;
    int16_t sy = wy - cam_y;
    return (sx > -16 && sx < SCREEN_W + 16 && sy > -16 && sy < PLAY_H + 16);
}

void render_update_sprites(void) {
    uint8_t i, spr_tile, spr_prop;
    uint8_t px, py;

    // --- Player ---
    if (game_state == STATE_DYING && !(death_timer & 2)) {
        move_sprite(OAM_PLAYER, 0, 0);
        move_sprite(OAM_PROP, 0, 0);
    } else {
        px = SPR_SCR_X(player_px);
        py = SPR_SCR_Y(player_py);

        if (!player_on_ground || player_thrusting) {
            spr_tile = SPR_PLAYER_FLY;
        } else if (player_vx != 0) {
            spr_tile = (anim_tick & 8) ? SPR_PLAYER_WALK : SPR_PLAYER_R;
        } else {
            spr_tile = SPR_PLAYER_R;
        }

        set_sprite_tile(OAM_PLAYER, spr_tile);
        set_sprite_prop(OAM_PLAYER, PAL_SP_PLAYER |
                        ((player_facing < 0) ? S_FLIPX : 0));
        move_sprite(OAM_PLAYER, px, py);

        if (!player_on_ground || player_thrusting) {
            spr_prop = (anim_tick & 4) ? SPR_PROP0 : SPR_PROP1;
            set_sprite_tile(OAM_PROP, spr_prop);
            set_sprite_prop(OAM_PROP, PAL_SP_LASER);
            move_sprite(OAM_PROP, px, py - 16);
        } else {
            move_sprite(OAM_PROP, 0, 0);
        }
    }

    // --- Active enemies ---
    for (i = 0; i < MAX_ACTIVE_ENEMIES; i++) {
        uint8_t oam = OAM_ENEMY0 + i;
        if (i < active_enemy_count && active_enemies[i].alive) {
            ActiveEnemy *ae = &active_enemies[i];
            if (sprite_visible(ae->px, ae->py)) {
                uint8_t ex = SPR_SCR_X(ae->px);
                uint8_t ey = SPR_SCR_Y(ae->py);

                if (ae->type == ENEMY_SPIDER) {
                    set_sprite_tile(oam, SPR_SPIDER);
                    set_sprite_prop(oam, PAL_SP_SPIDER);
                } else if (ae->type == ENEMY_SNAKE) {
                    set_sprite_tile(oam, SPR_SNAKE_R);
                    set_sprite_prop(oam, PAL_SP_SNAKE |
                                    ((ae->vx < 0) ? S_FLIPX : 0));
                } else {
                    set_sprite_tile(oam, (ae->anim & 8) ? SPR_BAT0 : SPR_BAT1);
                    set_sprite_prop(oam, PAL_SP_BAT);
                }
                move_sprite(oam, ex, ey);
            } else {
                move_sprite(oam, 0, 0);
            }
        } else {
            move_sprite(oam, 0, 0);
        }
    }

    // --- Miner ---
    if (miner_active && sprite_visible(miner_px, miner_py)) {
        set_sprite_tile(OAM_MINER, SPR_MINER);
        set_sprite_prop(OAM_MINER, PAL_SP_MINER);
        move_sprite(OAM_MINER, SPR_SCR_X(miner_px), SPR_SCR_Y(miner_py));
    } else {
        move_sprite(OAM_MINER, 0, 0);
    }

    // --- Dynamite / explosion ---
    if (dyn_active) {
        if (sprite_visible(dyn_px, dyn_py)) {
            if (dyn_exploding) {
                set_sprite_tile(OAM_DYNAMITE, SPR_EXPLODE);
                set_sprite_prop(OAM_DYNAMITE, PAL_SP_LASER);
            } else {
                set_sprite_tile(OAM_DYNAMITE, SPR_DYNAMITE);
                set_sprite_prop(OAM_DYNAMITE, PAL_SP_LASER);
            }
            move_sprite(OAM_DYNAMITE, SPR_SCR_X(dyn_px), SPR_SCR_Y(dyn_py));
        } else {
            move_sprite(OAM_DYNAMITE, 0, 0);
        }
    } else {
        move_sprite(OAM_DYNAMITE, 0, 0);
    }

    // --- Laser ---
    if (laser_active) {
        int16_t lx;
        uint8_t step = LASER_LENGTH / 3;
        for (i = 0; i < 3; i++) {
            lx = laser_px + (int16_t)laser_dir * step * (i + 1);
            if (sprite_visible(lx, laser_py)) {
                set_sprite_tile(OAM_LASER0 + i, SPR_LASER);
                set_sprite_prop(OAM_LASER0 + i, PAL_SP_LASER);
                move_sprite(OAM_LASER0 + i, SPR_SCR_X(lx), SPR_SCR_Y(laser_py));
            } else {
                move_sprite(OAM_LASER0 + i, 0, 0);
            }
        }
    } else {
        for (i = 0; i < 3; i++)
            move_sprite(OAM_LASER0 + i, 0, 0);
    }
}

// =========================================================================
// Utility
// =========================================================================

void render_hide_sprites(void) {
    uint8_t i;
    for (i = 0; i < OAM_COUNT; i++)
        move_sprite(i, 0, 0);
}

// =========================================================================
// Text helper: char -> tile index
// =========================================================================

static uint8_t char_to_tile(char ch) {
    if (ch >= '0' && ch <= '9') return TILE_DIGIT_0 + (ch - '0');
    switch (ch) {
        case 'A': return TILE_LETTER_A;
        case 'B': return TILE_LETTER_B;
        case 'C': return TILE_LETTER_C;
        case 'D': return TILE_LETTER_D;
        case 'E': return TILE_LETTER_E;
        case 'F': return TILE_LETTER_F;
        case 'G': return TILE_LETTER_G;
        case 'H': return TILE_LETTER_H;
        case 'I': return TILE_LETTER_I;
        case 'L': return TILE_LETTER_L;
        case 'N': return TILE_LETTER_N;
        case 'O': return TILE_LETTER_O;
        case 'P': return TILE_LETTER_P;
        case 'R': return TILE_LETTER_R;
        case 'S': return TILE_LETTER_S;
        case 'T': return TILE_LETTER_T;
        case 'U': return TILE_LETTER_U;
        case 'V': return TILE_LETTER_V;
        case '-': return TILE_LETTER_DASH;
        case ':': return TILE_COLON;
        case '.': return TILE_DOT;
        default:  return TILE_EMPTY;
    }
}

static void draw_text_row(uint8_t row, const char *text, uint8_t pal) {
    uint8_t i = 0, c;
    while (text[i]) i++;
    c = (20 - i) / 2;
    for (i = 0; text[i] && c < 20; i++, c++) {
        tilemap[row][c] = char_to_tile(text[i]);
        attrmap[row][c] = pal;
    }
}

static void clear_screen_tiles(void) {
    uint8_t r, c;
    for (r = 0; r < 18; r++)
        for (c = 0; c < 20; c++) {
            tilemap[r][c] = TILE_EMPTY;
            attrmap[r][c] = PAL_BG_EMPTY;
        }
}

static void upload_full_screen(void) {
    uint8_t r;
    SCX_REG = 0;
    SCY_REG = 0;
    for (r = 0; r < 18; r++) {
        VBK_REG = 0;
        set_bkg_tiles(0, r, 20, 1, &tilemap[r][0]);
        VBK_REG = 1;
        set_bkg_tiles(0, r, 20, 1, &attrmap[r][0]);
    }
    VBK_REG = 0;
}

// =========================================================================
// Title screen
// =========================================================================

void render_title(void) {
    uint8_t c;

    render_hide_sprites();

    clear_screen_tiles();

    // Decorative bars
    for (c = 0; c < 20; c++) {
        tilemap[3][c] = TILE_WALL + 0x0F;
        tilemap[14][c] = TILE_WALL + 0x0F;
        attrmap[3][c] = PAL_BG_CAVE;
        attrmap[14][c] = PAL_BG_CAVE;
    }

    draw_text_row(7, "R.E.S.C.U.E.", PAL_BG_DWALL);
    draw_text_row(11, "PRESS START", PAL_BG_HUD2);

    upload_full_screen();
}

// =========================================================================
// Message screen (level intro, game over, etc.)
// =========================================================================

void render_msg(const char *line1, const char *line2) {
    uint8_t c;

    render_hide_sprites();

    clear_screen_tiles();

    // Decorative bars
    for (c = 0; c < 20; c++) {
        tilemap[5][c] = TILE_WALL + 0x0A;
        tilemap[12][c] = TILE_WALL + 0x0A;
        attrmap[5][c] = PAL_BG_CAVE;
        attrmap[12][c] = PAL_BG_CAVE;
    }

    draw_text_row(8, line1, PAL_BG_DWALL);
    if (line2 != 0)
        draw_text_row(9, line2, PAL_BG_HUD);

    upload_full_screen();
}
