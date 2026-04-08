//
// render.c — Scrolling tilemap renderer, window HUD, sprites
//

#include "game.h"
#include "tiles.h"

// Window layer at bottom (WY=128) for HUD.
// Background uses full 32x32 VRAM as scrolling ring buffer via SCX/SCY.
// Only 1 new row/column written per tile scroll step.

static uint8_t hud_tiles[2][20];
static uint8_t hud_attrs[2][20];

// =========================================================================
// Tile-to-palette mapping
// =========================================================================

static uint8_t tile_palette(uint8_t tile_idx) {
    (void)tile_idx;
    return 0;
}

// =========================================================================
// Stream a single row into the VRAM ring buffer
// =========================================================================

static void stream_row(uint8_t level_row) {
    uint8_t vr = level_row & (VRAM_MAP_H - 1);
    uint8_t buf_tile[VRAM_MAP_W];
    uint8_t buf_attr[VRAM_MAP_W];
    uint8_t x;

    // Fill full 32-tile row (level tiles + solid padding)
    for (x = 0; x < VRAM_MAP_W; x++) {
        uint8_t lx = cam_tx + x;
        uint8_t t;
        if (lx < level_w && level_row < level_h)
            t = tile_at(lx, level_row);
        else
            t = 1;
        buf_tile[x] = t;
        buf_attr[x] = tile_palette(t);
    }
    VBK_REG = 0;
    set_bkg_tiles(0, vr, VRAM_MAP_W, 1, buf_tile);
    VBK_REG = 1;
    set_bkg_tiles(0, vr, VRAM_MAP_W, 1, buf_attr);
    VBK_REG = 0;
}

// =========================================================================
// Stream a single column into the VRAM ring buffer
// =========================================================================

static void stream_column(uint8_t level_col) {
    uint8_t vc = level_col & (VRAM_MAP_W - 1);
    uint8_t y;
    uint8_t t;

    for (y = 0; y < PLAY_ROWS + 2; y++) {
        uint8_t ly = cam_ty + y;
        uint8_t vr = ly & (VRAM_MAP_H - 1);
        if (level_col < level_w && ly < level_h)
            t = tile_at(level_col, ly);
        else
            t = 1;
        VBK_REG = 0;
        set_bkg_tiles(vc, vr, 1, 1, &t);
        uint8_t a = tile_palette(t);
        VBK_REG = 1;
        set_bkg_tiles(vc, vr, 1, 1, &a);
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
    if (max_cx < 0) max_cx = 0;
    if (max_cy < 0) max_cy = 0;
    if (cam_x > max_cx) cam_x = max_cx;
    if (cam_y > max_cy) cam_y = max_cy;

    cam_tx = (uint8_t)(cam_x >> 3);
    cam_ty = (uint8_t)(cam_y >> 3);

    // Fill all 32 VRAM rows with level tile data (ring buffer)
    {
        uint8_t buf_tile[VRAM_MAP_W];
        uint8_t buf_attr[VRAM_MAP_W];
        uint8_t y, x;
        for (y = 0; y < VRAM_MAP_H; y++) {
            uint8_t ly = cam_ty + y;
            for (x = 0; x < VRAM_MAP_W; x++) {
                uint8_t lx = cam_tx + x;
                uint8_t t;
                if (lx < level_w && ly < level_h)
                    t = tile_at(lx, ly);
                else
                    t = 1;
                buf_tile[x] = t;
                buf_attr[x] = tile_palette(t);
            }
            uint8_t vr = ly & (VRAM_MAP_H - 1);
            VBK_REG = 0;
            set_bkg_tiles(0, vr, VRAM_MAP_W, 1, buf_tile);
            VBK_REG = 1;
            set_bkg_tiles(0, vr, VRAM_MAP_W, 1, buf_attr);
        }
        VBK_REG = 0;
    }

    // Scroll registers: pixel offset into the ring buffer
    SCX_REG = (cam_tx * 8) & 0xFF;
    SCY_REG = (cam_ty * 8) & 0xFF;

    // Window at bottom for HUD (2 rows = 16 pixels)
    move_win(7, PLAY_H);  // WY = 128, WX = 7
    SHOW_WIN;

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

    // Stream new rows when camera moves vertically (1 row per step)
    while (cam_ty != new_ty) {
        if (new_ty > cam_ty) {
            cam_ty++;
            stream_row(cam_ty + PLAY_ROWS);  // new row below viewport
        } else {
            cam_ty--;
            stream_row(cam_ty);  // new row above viewport
        }
    }

    // Stream new columns when camera moves horizontally
    while (cam_tx != new_tx) {
        if (new_tx > cam_tx) {
            cam_tx++;
            stream_column(cam_tx + PLAY_COLS);  // new col right of viewport
        } else {
            cam_tx--;
            stream_column(cam_tx);  // new col left of viewport
        }
    }

    // Pixel-level scroll
    SCX_REG = cam_x & 0xFF;
    SCY_REG = cam_y & 0xFF;
}

// =========================================================================
// Clear a tile (for destroyed walls)
// =========================================================================

void render_clear_tile(uint8_t tx, uint8_t ty) {
    uint8_t vx = tx & (VRAM_MAP_W - 1);
    uint8_t vy = ty & (VRAM_MAP_H - 1);
    uint8_t empty = TILE_EMPTY;
    uint8_t attr = PAL_BG_EMPTY;
    VBK_REG = 0;
    set_bkg_tiles(vx, vy, 1, 1, &empty);
    VBK_REG = 1;
    set_bkg_tiles(vx, vy, 1, 1, &attr);
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

    // Upload to window layer (bottom of screen)
    VBK_REG = 0;
    set_win_tiles(0, 0, 20, 1, &hud_tiles[0][0]);
    set_win_tiles(0, 1, 20, 1, &hud_tiles[1][0]);
    VBK_REG = 1;
    set_win_tiles(0, 0, 20, 1, &hud_attrs[0][0]);
    set_win_tiles(0, 1, 20, 1, &hud_attrs[1][0]);
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

// Screen buffers for title/message screens (20 wide, 18 rows)
static uint8_t scr_tiles[18][20];
static uint8_t scr_attrs[18][20];

static void draw_text_row(uint8_t row, const char *text, uint8_t pal) {
    uint8_t i = 0, c;
    while (text[i]) i++;
    c = (20 - i) / 2;
    for (i = 0; text[i] && c < 20; i++, c++) {
        scr_tiles[row][c] = char_to_tile(text[i]);
        scr_attrs[row][c] = pal;
    }
}

static void clear_screen_tiles(void) {
    uint8_t r, c;
    for (r = 0; r < 18; r++)
        for (c = 0; c < 20; c++) {
            scr_tiles[r][c] = TILE_EMPTY;
            scr_attrs[r][c] = PAL_BG_EMPTY;
        }
}

static void upload_full_screen(void) {
    uint8_t r;
    HIDE_WIN;
    SCX_REG = 0;
    SCY_REG = 0;
    for (r = 0; r < 18; r++) {
        VBK_REG = 0;
        set_bkg_tiles(0, r, 20, 1, &scr_tiles[r][0]);
        VBK_REG = 1;
        set_bkg_tiles(0, r, 20, 1, &scr_attrs[r][0]);
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
        scr_tiles[3][c] = TILE_WALL + 0x0F;
        scr_tiles[14][c] = TILE_WALL + 0x0F;
        scr_attrs[3][c] = PAL_BG_CAVE;
        scr_attrs[14][c] = PAL_BG_CAVE;
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
        scr_tiles[5][c] = TILE_WALL + 0x0A;
        scr_tiles[12][c] = TILE_WALL + 0x0A;
        scr_attrs[5][c] = PAL_BG_CAVE;
        scr_attrs[12][c] = PAL_BG_CAVE;
    }

    draw_text_row(8, line1, PAL_BG_DWALL);
    if (line2 != 0)
        draw_text_row(9, line2, PAL_BG_HUD);

    upload_full_screen();
}
