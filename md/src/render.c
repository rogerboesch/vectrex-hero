/*
 * render.c -- VDP rendering with dual-plane parallax scrolling
 *
 * Plane A: foreground game tiles (full speed scroll)
 * Plane B: cave texture background (half speed scroll)
 * Window:  HUD (fixed, top 2 rows)
 */

#include "game.h"
#include "tiles.h"

/* =========================================================================
 * Tile-to-palette mapping
 * ========================================================================= */

static void draw_text_centered(u8 row, const char *text);
static u8 char_to_tile(char ch);

static u8 tile_palette(u8 tile_idx) {
    if (tile_idx == 0) return PAL0;
    if (tile_idx < 64) return PAL1;       /* walls */
    if (tile_idx < 80) return PAL2;       /* destroyable */
    if (tile_idx < 96) return PAL0;       /* decorative */
    return PAL3;                           /* HUD/chars */
}

/* =========================================================================
 * Stream a row into Plane A (foreground)
 * ========================================================================= */

static void stream_row(u8 level_row) {
    u8 vr = level_row & (PLANE_H - 1);
    u16 x;

    for (x = 0; x < PLAY_COLS; x++) {
        u8 lx = cam_tx + x;
        u8 t;
        u16 attr;

        if (lx < level_w && level_row < level_h)
            t = tile_at(lx, level_row);
        else
            t = 1;

        attr = TILE_ATTR_FULL(tile_palette(t), 0, FALSE, FALSE,
                              TILE_USER_BASE + t);
        VDP_setTileMapXY(BG_A, attr, x, vr + HUD_ROWS);
    }
}

/* =========================================================================
 * Stream a column into Plane A
 * ========================================================================= */

static void stream_column(u8 level_col) {
    u8 y;

    for (y = 0; y < PLAY_ROWS; y++) {
        u8 ly = cam_ty + y;
        u8 vr = ly & (PLANE_H - 1);
        u8 t;
        u16 attr;
        u8 scr_col = level_col - cam_tx;

        if (scr_col >= PLAY_COLS) continue;
        if (level_col < level_w && ly < level_h)
            t = tile_at(level_col, ly);
        else
            t = 1;

        attr = TILE_ATTR_FULL(tile_palette(t), 0, FALSE, FALSE,
                              TILE_USER_BASE + t);
        VDP_setTileMapXY(BG_A, attr, scr_col, vr + HUD_ROWS);
    }
}

/* =========================================================================
 * Fill Plane B with parallax cave texture
 * ========================================================================= */

static void fill_parallax(void) {
    u16 x, y;
    u16 attr;

    for (y = 0; y < PLANE_H; y++) {
        for (x = 0; x < PLANE_W; x++) {
            /* Alternate between parallax tiles for subtle pattern */
            u8 t = PARALLAX_TILE_BASE + ((x ^ y) & (PARALLAX_TILE_COUNT - 1));
            attr = TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                                  TILE_USER_BASE + t);
            VDP_setTileMapXY(BG_B, attr, x, y);
        }
    }
}

/* =========================================================================
 * Initialize level rendering
 * ========================================================================= */

void render_init_level(void) {
    u16 attr;
    u8 x, y;

    cam_x = 0;
    cam_y = 0;
    cam_tx = 0;
    cam_ty = 0;

    VDP_clearPlane(BG_A, TRUE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);

    /* Draw level tiles from tile_at */
    for (y = 0; y < level_h && y < PLAY_ROWS; y++) {
        for (x = 0; x < level_w && x < PLAY_COLS; x++) {
            u8 t = tile_at(x, y);
            attr = TILE_ATTR_FULL(tile_palette(t), 0, FALSE, FALSE,
                                  TILE_USER_BASE + t);
            VDP_setTileMapXY(BG_A, attr, x, y + HUD_ROWS);
        }
    }

    /* HUD via SGDK text on Plane B */
    VDP_setTextPlane(BG_B);
    render_update_hud();
}

/* =========================================================================
 * Camera update with parallax
 * ========================================================================= */

void render_update_camera(void) {
    s16 target_x, target_y, max_cx, max_cy;
    u8 new_tx, new_ty;

    target_x = player_px - (SCREEN_W / 2);
    target_y = player_py - (PLAY_H / 2);

    max_cx = (s16)level_w * 8 - SCREEN_W;
    max_cy = (s16)level_h * 8 - PLAY_H;
    if (max_cx < 0) max_cx = 0;
    if (max_cy < 0) max_cy = 0;
    if (target_x < 0) target_x = 0;
    if (target_y < 0) target_y = 0;
    if (target_x > max_cx) target_x = max_cx;
    if (target_y > max_cy) target_y = max_cy;

    cam_x = target_x;
    cam_y = target_y;

    new_tx = (u8)(cam_x >> 3);
    new_ty = (u8)(cam_y >> 3);

    /* Stream new rows */
    while (cam_ty != new_ty) {
        if (new_ty > cam_ty) {
            cam_ty++;
            stream_row(cam_ty + PLAY_ROWS - 1);
        }
        else {
            cam_ty--;
            stream_row(cam_ty);
        }
    }

    /* Stream new columns */
    while (cam_tx != new_tx) {
        if (new_tx > cam_tx) {
            cam_tx++;
            stream_column(cam_tx + PLAY_COLS - 1);
        }
        else {
            cam_tx--;
            stream_column(cam_tx);
        }
    }

    /* Scroll Plane A (foreground) at full speed */
    VDP_setHorizontalScroll(BG_A, -cam_x);
    VDP_setVerticalScroll(BG_A, cam_y);

    /* Plane B: fixed (used for HUD) */
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);
}

/* =========================================================================
 * Clear a destroyed tile on Plane A
 * ========================================================================= */

void render_clear_tile(u8 tx, u8 ty) {
    u8 vr = ty & (PLANE_H - 1);
    u8 sx = tx - cam_tx;
    u16 attr;

    if (sx >= PLAY_COLS) return;
    attr = TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, TILE_USER_BASE + TILE_EMPTY);
    VDP_setTileMapXY(BG_A, attr, sx, vr + HUD_ROWS);
}

/* =========================================================================
 * HUD (Window plane, top 2 rows)
 * ========================================================================= */

static u8 char_to_tile(char ch) {
    if (ch >= '0' && ch <= '9') return TILE_DIGIT_0 + (ch - '0');
    if (ch >= 'A' && ch <= 'Z') return TILE_LETTER_A + (ch - 'A');
    if (ch == '-') return TILE_LETTER_DASH;
    if (ch == ':') return TILE_COLON;
    if (ch == '.') return TILE_DOT;
    return TILE_EMPTY;
}

void render_update_hud(void) {
    char buf[8];
    u8 filled, c;

    /* Use SGDK text system on Plane B (doesn't scroll) */
    VDP_setTextPlane(BG_B);

    /* Level number */
    buf[0] = '0' + ((current_level + 1) / 10);
    buf[1] = '0' + ((current_level + 1) % 10);
    buf[2] = 0;
    VDP_drawText(buf, 1, 0);

    /* Lives */
    buf[0] = '0' + player_lives;
    buf[1] = 0;
    VDP_drawText(buf, 8, 0);

    /* Dynamite */
    buf[0] = '0' + player_dynamite;
    buf[1] = 0;
    VDP_drawText(buf, 14, 0);

    /* Score */
    {
        s16 s = score;
        u8 d;
        for (d = 0; d < 4; d++) {
            buf[3 - d] = '0' + (s % 10);
            s /= 10;
        }
        buf[4] = 0;
        VDP_drawText(buf, 35, 0);
    }

    /* Fuel bar as text for now */
    filled = (u8)((u16)player_fuel * 20 / START_FUEL);
    for (c = 0; c < 20; c++)
        buf[0] = (c < filled) ? '=' : '-';
    /* Just show fuel number */
    buf[0] = '0' + (player_fuel / 100);
    buf[1] = '0' + ((player_fuel / 10) % 10);
    buf[2] = '0' + (player_fuel % 10);
    buf[3] = 0;
    VDP_drawText(buf, 18, 0);
}

/* =========================================================================
 * Sprite rendering
 * ========================================================================= */

/* Sprite VRAM tile indices (must match tiles.c) */
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

#define PAL_PLAYER  PAL1
#define PAL_ENEMY   PAL2
#define PAL_MINER   PAL1
#define PAL_LASER   PAL2

static u8 sprite_count;

static u8 spr_visible(s16 wx, s16 wy) {
    s16 sx = wx - cam_x;
    s16 sy = wy - cam_y;
    return (sx > -16 && sx < SCREEN_W + 16 && sy > -16 && sy < PLAY_H);
}

static void spr_add(s16 wx, s16 wy, u16 tiles, u8 pal, u8 fliph) {
    s16 sx = wx - cam_x - 8;
    s16 sy = wy - cam_y - 8 + (HUD_ROWS * 8);
    VDP_setSpriteFull(sprite_count,
        sx, sy,
        SPRITE_SIZE(2, 2),
        TILE_ATTR_FULL(pal, 1, FALSE, fliph, tiles),
        sprite_count + 1);
    sprite_count++;
}

void render_update_sprites(void) {
    u8 i;
    u16 player_tiles;

    sprite_count = 0;

    /* Player */
    if (game_state == STATE_PLAYING || game_state == STATE_DYING) {
        if (!player_on_ground || player_thrusting)
            player_tiles = SPR_TILES_PLAYER_FLY;
        else
            player_tiles = SPR_TILES_PLAYER;

        spr_add(player_px, player_py, player_tiles, PAL_PLAYER,
                player_facing < 0 ? TRUE : FALSE);

        /* Propeller when flying */
        if (!player_on_ground || player_thrusting) {
            u16 prop_tiles = (anim_tick & 4) ? SPR_TILES_PROP0 : SPR_TILES_PROP1;
            spr_add(player_px, player_py - 16, prop_tiles, PAL_LASER, FALSE);
        }
    }

    /* Enemies */
    for (i = 0; i < active_enemy_count && i < MAX_ACTIVE_ENEMIES; i++) {
        ActiveEnemy *ae = &active_enemies[i];
        if (!ae->alive) continue;
        if (!spr_visible(ae->px, ae->py)) continue;

        if (ae->type == ENEMY_SPIDER) {
            spr_add(ae->px, ae->py, SPR_TILES_SPIDER, PAL_ENEMY, FALSE);
        }
        else if (ae->type == ENEMY_SNAKE) {
            spr_add(ae->px, ae->py, SPR_TILES_SNAKE, PAL_ENEMY,
                    ae->vx < 0 ? TRUE : FALSE);
        }
        else {
            u16 bat_tiles = (ae->anim & 8) ? SPR_TILES_BAT0 : SPR_TILES_BAT1;
            spr_add(ae->px, ae->py, bat_tiles, PAL_ENEMY, FALSE);
        }
    }

    /* Miner */
    if (miner_active && spr_visible(miner_px, miner_py)) {
        spr_add(miner_px, miner_py, SPR_TILES_MINER, PAL_MINER, FALSE);
    }

    /* Dynamite / explosion */
    if (dyn_active && spr_visible(dyn_px, dyn_py)) {
        if (dyn_exploding)
            spr_add(dyn_px, dyn_py, SPR_TILES_EXPLODE, PAL_LASER, FALSE);
        else
            spr_add(dyn_px, dyn_py, SPR_TILES_DYNAMITE, PAL_LASER, FALSE);
    }

    /* Laser */
    if (laser_active) {
        s16 step = LASER_LENGTH / 3;
        for (i = 0; i < 3; i++) {
            s16 lx = laser_px + (s16)laser_dir * step * (i + 1);
            if (spr_visible(lx, laser_py))
                spr_add(lx, laser_py, SPR_TILES_LASER, PAL_LASER, FALSE);
        }
    }

    /* Terminate sprite list: set link=0 on last sprite */
    if (sprite_count > 0) {
        /* Re-write last sprite with link=0 to end the chain */
        /* We can't easily re-read it, so just add a dummy off-screen sprite with link=0 */
        VDP_setSpriteFull(sprite_count, 0, 0, 0, 0, 0);
        sprite_count++;
    }

    VDP_updateSprites(sprite_count, DMA);
}

void render_hide_sprites(void) {
    VDP_setSpriteFull(0, 0, 0, 0, 0, 0);
    VDP_updateSprites(1, DMA);
}

/* =========================================================================
 * Title and message screens
 * ========================================================================= */

static void draw_text_centered(u8 row, const char *text) {
    u8 len = 0;
    u8 col, i;
    u16 attr;
    const char *p = text;

    while (*p++) len++;
    col = (PLAY_COLS - len) / 2;

    for (i = 0; text[i] && col + i < PLAY_COLS; i++) {
        attr = TILE_ATTR_FULL(PAL3, 1, FALSE, FALSE,
                              TILE_USER_BASE + char_to_tile(text[i]));
        VDP_setTileMapXY(BG_A, attr, col + i, row);
    }
}

void render_title(void) {
    render_hide_sprites();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setWindowVPos(FALSE, 0);  /* hide window for title */

    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);

    draw_text_centered(10, "R.E.S.C.U.E.");
    draw_text_centered(16, "PRESS START");
}

void render_msg(const char *line1, const char *line2) {
    render_hide_sprites();
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setWindowVPos(FALSE, 0);

    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);

    draw_text_centered(11, line1);
    if (line2)
        draw_text_centered(13, line2);
}
