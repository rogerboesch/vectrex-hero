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

        attr = TILE_ATTR_FULL(tile_palette(t), 1, FALSE, FALSE,
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

        attr = TILE_ATTR_FULL(tile_palette(t), 1, FALSE, FALSE,
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
            attr = TILE_ATTR_FULL(tile_palette(t), 1, FALSE, FALSE,
                                  TILE_USER_BASE + t);
            VDP_setTileMapXY(BG_A, attr, x, y + HUD_ROWS);
        }
    }

    /* Minimal HUD test */
    VDP_setWindowHPos(FALSE, 0);
    VDP_setWindowVPos(TRUE, HUD_ROWS);
    {
        u16 hattr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE,
                                    TILE_USER_BASE + TILE_DIGIT_0 + 1);
        VDP_setTileMapXY(WINDOW, hattr, 0, 0);
    }
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

    /* Scroll Plane B (parallax) at half speed */
    VDP_setHorizontalScroll(BG_B, -(cam_x >> 1));
    VDP_setVerticalScroll(BG_B, cam_y >> 1);
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
    u8 c;
    u16 attr;

    /* Clear HUD rows on window plane */
    for (c = 0; c < PLAY_COLS; c++) {
        attr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE, TILE_USER_BASE + TILE_EMPTY);
        VDP_setTileMapXY(WINDOW, attr, c, 0);
        VDP_setTileMapXY(WINDOW, attr, c, 1);
    }

    /* Row 0: level number */
    attr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE,
                          TILE_USER_BASE + TILE_DIGIT_0 + ((current_level + 1) / 10));
    VDP_setTileMapXY(WINDOW, attr, 0, 0);
    attr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE,
                          TILE_USER_BASE + TILE_DIGIT_0 + ((current_level + 1) % 10));
    VDP_setTileMapXY(WINDOW, attr, 1, 0);

    /* Hearts */
    for (c = 0; c < 3; c++) {
        u8 t = (player_lives >= c + 1) ? TILE_HEART : TILE_HEART_OFF;
        attr = TILE_ATTR_FULL(PAL2, 0, FALSE, FALSE, TILE_USER_BASE + t);
        VDP_setTileMapXY(WINDOW, attr, 6 + c, 0);
    }

    /* Dynamite */
    for (c = 0; c < 3; c++) {
        u8 t = (player_dynamite >= c + 1) ? TILE_DYN_ICON : TILE_DYN_OFF;
        attr = TILE_ATTR_FULL(PAL2, 0, FALSE, FALSE, TILE_USER_BASE + t);
        VDP_setTileMapXY(WINDOW, attr, 11 + c, 0);
    }

    /* Score (right-aligned) */
    {
        s16 s = score;
        u8 d;
        for (d = 0; d < 4; d++) {
            attr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE,
                                  TILE_USER_BASE + TILE_DIGIT_0 + (s % 10));
            VDP_setTileMapXY(WINDOW, attr, 39 - d, 0);
            s /= 10;
        }
    }

    /* Row 1: fuel bar (centered, 20 tiles) */
    {
        u8 filled = (u8)((u16)player_fuel * 20 / START_FUEL);
        for (c = 0; c < 20; c++) {
            u8 t = (c < filled) ? TILE_HUD_FILL : TILE_HUD_EMPTY;
            attr = TILE_ATTR_FULL(PAL3, 0, FALSE, FALSE, TILE_USER_BASE + t);
            VDP_setTileMapXY(WINDOW, attr, 10 + c, 1);
        }
    }
}

/* =========================================================================
 * Sprite rendering
 * ========================================================================= */

void render_update_sprites(void) {
    /* TODO: implement MD sprite rendering using SGDK SPR_* API */
    /* For now, placeholder */
}

void render_hide_sprites(void) {
    SPR_reset();
    SPR_update();
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
