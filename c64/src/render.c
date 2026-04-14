/*
 * render.c — VIC-II tile rendering, smooth scroll, HUD, sprites
 *
 * Uses custom charset at $3000. Screen RAM at $0400 (default).
 * Smooth scroll via VIC-II CTRL1/CTRL2 registers (3-bit X/Y).
 * Streams new rows/columns into screen RAM as camera moves.
 */

#include "game.h"
#include "tiles.h"

/* =========================================================================
 * Tile-to-color mapping
 * ========================================================================= */

static uint8_t tile_color(uint8_t tile_idx) {
    if (tile_idx == 0) return COLOR_BLACK;
    if (tile_idx < 64) return COLOR_LIGHTBLUE;
    if (tile_idx < 80) return COLOR_YELLOW;      /* destroyable */
    if (tile_idx < 96) return COLOR_DARKGRAY;     /* decorative */
    return COLOR_WHITE;                            /* HUD/chars */
}

/* =========================================================================
 * Stream a single row into screen RAM
 * ========================================================================= */

static void stream_row(uint8_t level_row) {
    uint8_t scr_row = (level_row - cam_ty) + HUD_ROWS;
    uint8_t x;
    uint16_t offset;

    if (scr_row >= 25) return;
    offset = (uint16_t)scr_row * 40;

    for (x = 0; x < PLAY_COLS; x++) {
        uint8_t lx = cam_tx + x;
        uint8_t t;
        if (lx < level_w && level_row < level_h)
            t = tile_at(lx, level_row);
        else
            t = 1;  /* solid */
        SCREEN_RAM[offset + x] = t;
        COLOR_RAM[offset + x] = tile_color(t);
    }
}

/* =========================================================================
 * Stream a single column into screen RAM
 * ========================================================================= */

static void stream_column(uint8_t level_col) {
    uint8_t y;
    for (y = 0; y < PLAY_ROWS; y++) {
        uint8_t ly = cam_ty + y;
        uint8_t scr_row = y + HUD_ROWS;
        uint8_t t;
        uint16_t offset = (uint16_t)scr_row * 40;
        uint8_t scr_col = level_col - cam_tx;

        if (scr_col >= PLAY_COLS) continue;
        if (level_col < level_w && ly < level_h)
            t = tile_at(level_col, ly);
        else
            t = 1;
        SCREEN_RAM[offset + scr_col] = t;
        COLOR_RAM[offset + scr_col] = tile_color(t);
    }
}

/* =========================================================================
 * Initialize level rendering
 * ========================================================================= */

void render_init_level(void) {
    uint8_t y, x;

    cam_x = player_px - (SCREEN_W / 2);
    cam_y = player_py - (PLAY_H / 2);

    /* Clamp camera */
    {
        int16_t max_cx = (int16_t)level_w * 8 - SCREEN_W;
        int16_t max_cy = (int16_t)level_h * 8 - PLAY_H;
        if (max_cx < 0) max_cx = 0;
        if (max_cy < 0) max_cy = 0;
        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_x > max_cx) cam_x = max_cx;
        if (cam_y > max_cy) cam_y = max_cy;
    }

    cam_tx = (uint8_t)(cam_x >> 3);
    cam_ty = (uint8_t)(cam_y >> 3);

    /* Clear screen */
    memset(SCREEN_RAM, TILE_EMPTY, 1000);
    memset(COLOR_RAM, COLOR_BLACK, 1000);

    /* Fill visible rows */
    for (y = 0; y < PLAY_ROWS; y++) {
        uint8_t ly = cam_ty + y;
        uint8_t scr_row = y + HUD_ROWS;
        uint16_t offset = (uint16_t)scr_row * 40;

        for (x = 0; x < PLAY_COLS; x++) {
            uint8_t lx = cam_tx + x;
            uint8_t t;
            if (lx < level_w && ly < level_h)
                t = tile_at(lx, ly);
            else
                t = 1;
            SCREEN_RAM[offset + x] = t;
            COLOR_RAM[offset + x] = tile_color(t);
        }
    }

    /* Set smooth scroll to pixel offset */
    VIC_CTRL1 = (VIC_CTRL1 & 0xF8) | (cam_y & 7);
    VIC_CTRL2 = (VIC_CTRL2 & 0xF8) | (cam_x & 7);

    render_update_hud();
}

/* =========================================================================
 * Camera update
 * ========================================================================= */

void render_update_camera(void) {
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

    /* Stream new rows when camera moves vertically */
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

    /* Stream new columns when camera moves horizontally */
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

    /* Smooth scroll (pixel offset within tile) */
    VIC_CTRL1 = (VIC_CTRL1 & 0xF8) | (cam_y & 7);
    VIC_CTRL2 = (VIC_CTRL2 & 0xF8) | (cam_x & 7);
}

/* =========================================================================
 * Clear a destroyed tile
 * ========================================================================= */

void render_clear_tile(uint8_t tx, uint8_t ty) {
    uint8_t sx = tx - cam_tx;
    uint8_t sy = ty - cam_ty + HUD_ROWS;
    if (sx < PLAY_COLS && sy < 25) {
        uint16_t offset = (uint16_t)sy * 40 + sx;
        SCREEN_RAM[offset] = TILE_EMPTY;
        COLOR_RAM[offset] = COLOR_BLACK;
    }
}

/* =========================================================================
 * HUD (top 2 rows of screen RAM)
 * ========================================================================= */

static uint8_t char_to_tile(char ch) {
    if (ch >= '0' && ch <= '9') return TILE_DIGIT_0 + (ch - '0');
    if (ch >= 'A' && ch <= 'Z') return TILE_LETTER_A + (ch - 'A');
    if (ch == '-') return TILE_LETTER_DASH;
    if (ch == ':') return TILE_COLON;
    if (ch == '.') return TILE_DOT;
    return TILE_EMPTY;
}

void render_update_hud(void) {
    uint8_t c;

    /* Clear HUD rows */
    memset(SCREEN_RAM, TILE_EMPTY, 40);
    memset(SCREEN_RAM + 40, TILE_EMPTY, 40);
    memset(COLOR_RAM, COLOR_WHITE, 40);
    memset(COLOR_RAM + 40, COLOR_GREEN, 40);

    /* Row 0: level, hearts, dynamite, score */
    SCREEN_RAM[0] = TILE_DIGIT_0 + ((current_level + 1) / 10);
    SCREEN_RAM[1] = TILE_DIGIT_0 + ((current_level + 1) % 10);

    /* Hearts (centered) */
    SCREEN_RAM[6]  = (player_lives >= 1) ? TILE_HEART : TILE_HEART_OFF;
    SCREEN_RAM[7]  = (player_lives >= 2) ? TILE_HEART : TILE_HEART_OFF;
    SCREEN_RAM[8]  = (player_lives >= 3) ? TILE_HEART : TILE_HEART_OFF;
    COLOR_RAM[6] = COLOR_RED;
    COLOR_RAM[7] = COLOR_RED;
    COLOR_RAM[8] = COLOR_RED;

    /* Dynamite */
    SCREEN_RAM[11] = (player_dynamite >= 1) ? TILE_DYN_ICON : TILE_DYN_OFF;
    SCREEN_RAM[12] = (player_dynamite >= 2) ? TILE_DYN_ICON : TILE_DYN_OFF;
    SCREEN_RAM[13] = (player_dynamite >= 3) ? TILE_DYN_ICON : TILE_DYN_OFF;
    COLOR_RAM[11] = COLOR_YELLOW;
    COLOR_RAM[12] = COLOR_YELLOW;
    COLOR_RAM[13] = COLOR_YELLOW;

    /* Score (right-aligned) */
    {
        int16_t s = score;
        uint8_t d;
        for (d = 0; d < 4; d++) {
            SCREEN_RAM[39 - d] = TILE_DIGIT_0 + (s % 10);
            s /= 10;
        }
    }

    /* Row 1: fuel bar (centered, 20 chars wide) */
    {
        uint8_t filled = (uint8_t)((uint16_t)player_fuel * 20 / START_FUEL);
        for (c = 0; c < 20; c++) {
            SCREEN_RAM[40 + 10 + c] = (c < filled) ? TILE_HUD_FILL : TILE_HUD_EMPTY;
            COLOR_RAM[40 + 10 + c] = COLOR_GREEN;
        }
    }
}

/* =========================================================================
 * Sprite rendering (VIC-II hardware sprites)
 * ========================================================================= */

void render_update_sprites(void) {
    /* TODO: implement C64 hardware sprite rendering */
    /* Sprite X: 0xD000 + sprite*2, Y: 0xD001 + sprite*2 */
    /* For now, just position player sprite */
    uint16_t sx = SPR_SCR_X(player_px);
    uint8_t sy = SPR_SCR_Y(player_py);

    SPR_ENABLE = 0x01;  /* enable sprite 0 only */
    *(uint8_t*)(0xD000) = sx & 0xFF;
    *(uint8_t*)(0xD001) = sy;
    if (sx > 255)
        SPR_XMSB |= 0x01;
    else
        SPR_XMSB &= ~0x01;

    *(uint8_t*)(0xD027) = COLOR_GREEN;  /* sprite 0 color */
}

void render_hide_sprites(void) {
    SPR_ENABLE = 0;
}

/* =========================================================================
 * Title and message screens
 * ========================================================================= */

static void draw_text_centered(uint8_t row, const char *text) {
    uint8_t len = 0;
    uint8_t col, i;
    uint16_t offset;
    const char *p = text;

    while (*p++) len++;
    col = (40 - len) / 2;
    offset = (uint16_t)row * 40 + col;

    for (i = 0; text[i] && col + i < 40; i++) {
        SCREEN_RAM[offset + i] = char_to_tile(text[i]);
        COLOR_RAM[offset + i] = COLOR_WHITE;
    }
}

void render_title(void) {
    render_hide_sprites();
    memset(SCREEN_RAM, TILE_EMPTY, 1000);
    memset(COLOR_RAM, COLOR_BLACK, 1000);

    draw_text_centered(8, "R.E.S.C.U.E.");
    draw_text_centered(14, "PRESS FIRE");
}

void render_msg(const char *line1, const char *line2) {
    render_hide_sprites();
    memset(SCREEN_RAM, TILE_EMPTY, 1000);
    memset(COLOR_RAM, COLOR_BLACK, 1000);

    draw_text_centered(10, line1);
    if (line2)
        draw_text_centered(12, line2);
}
