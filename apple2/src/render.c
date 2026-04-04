/*
 * render.c -- Apple II Hi-Res rendering engine
 *
 * Target: 6502 @ 1.023MHz, 280x192 Hi-Res, monochrome white sprites.
 *
 * RENDERING ARCHITECTURE (adapted from QL port):
 *
 *   Uses background-restore (save-behind) approach:
 *
 *   1. On room entry: render cave geometry to Hi-Res page 2 ($4000,
 *      the background buffer). Then copy page 2 to page 1 ($2000,
 *      the display).
 *
 *   2. Each frame: for each moving sprite:
 *      a. Restore previous position from page 2 (background)
 *      b. Draw sprite at new position on page 1
 *      c. Record new position for next frame
 *
 *   3. Skip optimization: if sprite hasn't moved, skip erase/draw.
 *
 * CAVE RASTERIZATION:
 *
 *   Same grid+flood-fill approach as QL, adapted for Hi-Res:
 *   - Grid: 40x24 cells (each cell = 7x7 pixels = 1 Hi-Res byte wide)
 *   - Trace cave polylines onto grid (Bresenham)
 *   - Flood fill from player start to mark interior as empty
 *   - Render: SOLID cells = white block, BORDER cells = white outline
 *
 * SPRITE SYSTEM:
 *
 *   Sprites use data+mask format. See sprites.h.
 *   For speed, sprite X positions are snapped to byte boundaries
 *   (multiples of 7 pixels). This avoids per-pixel bit shifting.
 */

#include "game.h"
#include "sprites.h"

/* ===================================================================
 * Cave grid for rasterization
 * =================================================================== */

#define GRID_W  40
#define GRID_H  24
#define CELL_W  7      /* pixels per cell (matches 1 Hi-Res byte) */
#define CELL_H  7

#define CELL_SOLID  2
#define CELL_BORDER 1
#define CELL_EMPTY  0

static uint8_t cave_grid[GRID_H][GRID_W];

/* ===================================================================
 * Save-behind slots
 * =================================================================== */

#define MAX_SLOTS   8

typedef struct {
    uint8_t x_byte;     /* byte offset in row */
    uint8_t y;          /* screen row */
    uint8_t wb;         /* width in bytes */
    uint8_t h;          /* height in pixels */
    uint8_t active;
} SaveSlot;

static SaveSlot slots[MAX_SLOTS];

/* Per-slot tracking to skip unchanged sprites */
static uint16_t slot_last_sx[MAX_SLOTS];
static uint8_t slot_last_sy[MAX_SLOTS];
static const uint8_t *slot_last_spr[MAX_SLOTS];
static uint8_t slot_drawn[MAX_SLOTS];

/* HUD dirty tracking */
static int16_t hud_last_score;
static uint8_t hud_last_fuel;
static uint8_t hud_last_lives;
static uint8_t hud_last_dyn;
static uint8_t hud_drawn;

/* ===================================================================
 * Hi-Res pixel plotting (C fallback -- used for cave lines)
 *
 * Hi-Res byte: bits 0-6 are pixels, bit 7 = palette select (0=white/green)
 * Byte N on a row covers pixels N*7 to N*7+6.
 * =================================================================== */

static void plot_pixel(uint8_t *page, uint16_t x, uint8_t y) {
    uint8_t byte_off, bit;
    uint8_t *row_base;
    uint16_t row_off;

    if (x >= SCREEN_W || y >= SCREEN_H) return;

    row_off = hgr_row_addr[y];
    row_base = page + row_off;
    byte_off = (uint8_t)(x / 7);
    bit = (uint8_t)(x % 7);
    row_base[byte_off] |= (1 << bit);
}

/* ===================================================================
 * Line drawing (Bresenham)
 * =================================================================== */

static void draw_line(uint8_t *page, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2) {
    int16_t dx, dy, sx, sy, err, e2;
    dx = x2 - x1; if (dx < 0) dx = -dx;
    dy = y2 - y1; if (dy < 0) dy = -dy;
    sx = x1 < x2 ? 1 : -1;
    sy = y1 < y2 ? 1 : -1;
    err = dx - dy;

    for (;;) {
        if (x1 >= 0 && x1 < (int16_t)SCREEN_W &&
            y1 >= 0 && y1 < (int16_t)SCREEN_H)
            plot_pixel(page, (uint16_t)x1, (uint8_t)y1);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

/* ===================================================================
 * Cave grid rasterization
 * =================================================================== */

static uint8_t grid_col(int8_t gx) {
    return (uint8_t)(((uint16_t)((uint8_t)(gx + 128)) * GRID_W) >> 8);
}

static uint8_t grid_row(int8_t gy) {
    return (uint8_t)(((uint16_t)(50 - gy) * (GRID_H - 1)) / 100);
}

/* Trace a line onto the cell grid (Bresenham) */
static void trace_grid_line(int8_t gx1, int8_t gy1, int8_t gx2, int8_t gy2) {
    int16_t c1, r1, c2, r2, dx, dy, sx, sy, err, e2;
    c1 = grid_col(gx1); r1 = grid_row(gy1);
    c2 = grid_col(gx2); r2 = grid_row(gy2);
    dx = c2 - c1; if (dx < 0) dx = -dx;
    dy = r2 - r1; if (dy < 0) dy = -dy;
    sx = c1 < c2 ? 1 : -1;
    sy = r1 < r2 ? 1 : -1;
    err = dx - dy;

    for (;;) {
        if ((uint8_t)r1 < GRID_H && (uint8_t)c1 < GRID_W)
            cave_grid[r1][c1] = CELL_BORDER;
        if (c1 == c2 && r1 == r2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; c1 += sx; }
        if (e2 < dx)  { err += dx; r1 += sy; }
    }
}

/* Trace all polylines onto grid */
static void trace_cave(void) {
    const int8_t *p = cur_cave_lines;
    uint8_t n, i;
    int8_t cx, cy;

    while ((n = (uint8_t)*p++) != 0) {
        cy = p[0]; cx = p[1]; p += 2;
        for (i = 0; i < n; i++) {
            int8_t ny = cy + p[0];
            int8_t nx = cx + p[1];
            trace_grid_line(cx, cy, nx, ny);
            cy = ny; cx = nx;
            p += 2;
        }
    }
}

/* Stack-based flood fill */
#define FILL_STACK_SIZE 256
static uint16_t fill_stack[FILL_STACK_SIZE];
static uint16_t fill_sp;

static void flood_fill(uint8_t sr, uint8_t sc) {
    uint8_t r, c;
    if (sr >= GRID_H || sc >= GRID_W) return;

    fill_sp = 0;
    fill_stack[fill_sp++] = ((uint16_t)sr << 8) | sc;

    while (fill_sp > 0) {
        uint16_t v = fill_stack[--fill_sp];
        r = (uint8_t)(v >> 8);
        c = (uint8_t)(v & 0xFF);
        if (cave_grid[r][c] != CELL_SOLID) continue;
        cave_grid[r][c] = CELL_EMPTY;
        if (r > 0           && cave_grid[r-1][c] == CELL_SOLID && fill_sp < FILL_STACK_SIZE)
            fill_stack[fill_sp++] = (uint16_t)((r-1) << 8) | c;
        if (r < GRID_H - 1  && cave_grid[r+1][c] == CELL_SOLID && fill_sp < FILL_STACK_SIZE)
            fill_stack[fill_sp++] = (uint16_t)((r+1) << 8) | c;
        if (c > 0           && cave_grid[r][c-1] == CELL_SOLID && fill_sp < FILL_STACK_SIZE)
            fill_stack[fill_sp++] = (uint16_t)(r << 8) | (c-1);
        if (c < GRID_W - 1  && cave_grid[r][c+1] == CELL_SOLID && fill_sp < FILL_STACK_SIZE)
            fill_stack[fill_sp++] = (uint16_t)(r << 8) | (c+1);
    }
}

/* Render grid cells to background page */
static void render_cave_cells(uint8_t *page) {
    uint8_t r, c, cell;
    uint8_t *row_base;
    uint16_t row_off;
    uint8_t py, px_byte;

    for (r = 0; r < GRID_H; r++) {
        for (c = 0; c < GRID_W; c++) {
            cell = cave_grid[r][c];
            if (cell == CELL_EMPTY) continue;

            /* Each cell is CELL_W x CELL_H pixels.
             * Cell (r,c) starts at pixel (c*CELL_W, PLAY_Y_OFF + r*CELL_H).
             * Since CELL_W=7, each cell is exactly 1 Hi-Res byte wide. */
            px_byte = c;  /* 1 byte per cell column */

            for (py = 0; py < CELL_H; py++) {
                uint8_t screen_y = PLAY_Y_OFF + r * CELL_H + py;
                if (screen_y >= SCREEN_H) break;
                row_off = hgr_row_addr[screen_y];
                row_base = page + row_off;

                if (cell == CELL_BORDER) {
                    /* Border: outline only (top/bottom/left/right edges) */
                    if (py == 0 || py == CELL_H - 1)
                        row_base[px_byte] = 0x7F;  /* full row of 7 white pixels */
                    else
                        row_base[px_byte] = 0x41;  /* left and right edge only */
                } else {
                    /* Solid rock: filled white block */
                    row_base[px_byte] = 0x7F;
                }
            }
        }
    }
}

/* Draw cave polylines directly (for visual detail on top of grid) */
static void render_cave_lines(uint8_t *page) {
    const int8_t *p = cur_cave_lines;
    uint8_t n, i;
    int8_t cx, cy;

    while ((n = (uint8_t)*p++) != 0) {
        cy = p[0]; cx = p[1]; p += 2;
        for (i = 0; i < n; i++) {
            int8_t ny = cy + p[0];
            int8_t nx = cx + p[1];

            if (p[0] == 0) {
                /* Horizontal segment -- use fast hline */
                uint16_t sx1 = SCREEN_X(cx);
                uint16_t sx2 = SCREEN_X(nx);
                uint8_t sy = (uint8_t)SCREEN_Y(cy);
                if (sx1 > sx2) { uint16_t t = sx1; sx1 = sx2; sx2 = t; }
                hgr_hline(page, sx1, sx2, sy);
            } else if (p[1] == 0) {
                /* Vertical segment -- use fast vline */
                uint16_t sx = SCREEN_X(cx);
                uint8_t sy1 = (uint8_t)SCREEN_Y(cy);
                uint8_t sy2 = (uint8_t)SCREEN_Y(ny);
                if (sy1 > sy2) { uint8_t t = sy1; sy1 = sy2; sy2 = t; }
                hgr_vline(page, sx, sy1, sy2);
            } else {
                /* Diagonal -- Bresenham */
                draw_line(page,
                          (int16_t)SCREEN_X(cx), (int16_t)SCREEN_Y(cy),
                          (int16_t)SCREEN_X(nx), (int16_t)SCREEN_Y(ny));
            }

            cy = ny; cx = nx;
            p += 2;
        }
    }
}

/* Render destroyable walls to background */
static void render_walls_bg(uint8_t *page) {
    uint8_t i;
    for (i = 0; i < cur_wall_count; i++) {
        uint16_t wx, ww;
        uint8_t wy, wh, r;
        if (walls_destroyed & (1 << i)) continue;
        wx = SCREEN_X(wall_x(i));
        wy = (uint8_t)SCREEN_Y(wall_y(i) + wall_h(i));
        ww = SCREEN_X(wall_x(i) + wall_w(i)) - wx;
        wh = (uint8_t)(SCREEN_Y(wall_y(i) - wall_h(i)) - wy);
        if (wh < 2) wh = 2;

        /* Draw wall as a filled rectangle with dashed pattern */
        for (r = 0; r < wh && (wy + r) < SCREEN_H; r++) {
            uint16_t row_off = hgr_row_addr[wy + r];
            uint8_t *row = page + row_off;
            uint8_t start_byte = (uint8_t)(wx / 7);
            uint8_t end_byte = (uint8_t)((wx + ww) / 7);
            uint8_t b;
            uint8_t pattern = (r & 1) ? 0x55 : 0x2A;  /* alternating */
            for (b = start_byte; b <= end_byte && b < SCREEN_STRIDE; b++)
                row[b] |= pattern;
        }
    }
}

/* Render lava strip */
static void render_lava_bg(uint8_t *page) {
    uint8_t r;
    uint8_t lava_y;
    if (!cur_has_lava) return;

    /* Find lowest cave point */
    {
        const int8_t *p = cur_cave_lines;
        uint8_t n;
        int8_t cy, min_y = 50;
        while ((n = (uint8_t)*p++) != 0) {
            uint8_t j;
            cy = p[0]; p += 2;
            if (cy < min_y) min_y = cy;
            for (j = 0; j < n; j++) {
                cy += p[0]; p += 2;
                if (cy < min_y) min_y = cy;
            }
        }
        lava_y = (uint8_t)SCREEN_Y(min_y) + 2;
    }

    /* Draw lava as dashed red-ish line pattern (white with gaps on HiRes) */
    for (r = 0; r < 4 && (lava_y + r) < SCREEN_H; r++) {
        uint16_t row_off = hgr_row_addr[lava_y + r];
        uint8_t *row = page + row_off;
        uint8_t b;
        uint8_t pattern = (r & 1) ? 0x55 : 0x2A;
        for (b = 0; b < SCREEN_STRIDE; b++)
            row[b] |= pattern;
    }
}

/* ===================================================================
 * Save-behind: restore from background page
 * =================================================================== */

static void restore_behind(uint8_t slot) {
    SaveSlot *s = &slots[slot];
    if (!s->active) return;
    hgr_restore_rect(HGR_PAGE2, HGR_PAGE1,
                     (uint16_t)s->x_byte, s->y, s->wb, s->h);
    s->active = 0;
}

static void mark_behind(uint8_t slot, uint8_t x_byte, uint8_t y,
                        uint8_t wb, uint8_t h) {
    SaveSlot *s = &slots[slot];
    s->x_byte = x_byte;
    s->y = y;
    s->wb = wb;
    s->h = h;
    s->active = 1;
}

/* ===================================================================
 * Sprite blitting with save-behind
 * =================================================================== */

static void blit_sprite_screen(const Sprite *spr, uint16_t sx, uint8_t sy,
                               uint8_t slot, uint8_t flip_h) {
    uint8_t x_byte, save_wb;
    (void)flip_h;  /* TODO: implement horizontal flip */

    x_byte = (uint8_t)(sx / 7);
    save_wb = spr->wb + 1;  /* +1 for alignment overshoot */
    if (x_byte + save_wb > SCREEN_STRIDE)
        save_wb = SCREEN_STRIDE - x_byte;

    if (sy < SCREEN_H) {
        mark_behind(slot, x_byte, sy, save_wb, spr->h);
        hgr_blit_sprite(HGR_PAGE1, spr->data, spr->mask,
                        (uint16_t)x_byte, sy, spr->wb, spr->h);
    }
}

/* Erase + draw sprite. Skip if unchanged. */
static void erase_draw(uint8_t slot, const Sprite *spr,
                       uint16_t sx, uint8_t sy, uint8_t flip) {
    if (slot_drawn[slot] && sx == slot_last_sx[slot]
        && sy == slot_last_sy[slot] && spr->data == slot_last_spr[slot])
        return;
    restore_behind(slot);
    blit_sprite_screen(spr, sx, sy, slot, flip);
    slot_last_sx[slot] = sx;
    slot_last_sy[slot] = sy;
    slot_last_spr[slot] = spr->data;
    slot_drawn[slot] = 1;
}

/* ===================================================================
 * Simple 4x6 bitmap font for HUD
 * =================================================================== */

static const uint8_t font_4x6[41][6] = {
    /* 0 */ {0x06, 0x09, 0x0B, 0x0D, 0x09, 0x06},
    /* 1 */ {0x02, 0x06, 0x02, 0x02, 0x02, 0x07},
    /* 2 */ {0x06, 0x09, 0x01, 0x06, 0x08, 0x0F},
    /* 3 */ {0x06, 0x09, 0x02, 0x01, 0x09, 0x06},
    /* 4 */ {0x01, 0x03, 0x05, 0x09, 0x0F, 0x01},
    /* 5 */ {0x0F, 0x08, 0x0E, 0x01, 0x09, 0x06},
    /* 6 */ {0x03, 0x04, 0x0E, 0x09, 0x09, 0x06},
    /* 7 */ {0x0F, 0x01, 0x02, 0x04, 0x04, 0x04},
    /* 8 */ {0x06, 0x09, 0x06, 0x09, 0x09, 0x06},
    /* 9 */ {0x06, 0x09, 0x09, 0x07, 0x01, 0x06},
    /* A */ {0x06, 0x09, 0x09, 0x0F, 0x09, 0x09},
    /* B */ {0x0E, 0x09, 0x0E, 0x09, 0x09, 0x0E},
    /* C */ {0x06, 0x09, 0x08, 0x08, 0x09, 0x06},
    /* D */ {0x0E, 0x09, 0x09, 0x09, 0x09, 0x0E},
    /* E */ {0x0F, 0x08, 0x0E, 0x08, 0x08, 0x0F},
    /* F */ {0x0F, 0x08, 0x0E, 0x08, 0x08, 0x08},
    /* G */ {0x06, 0x09, 0x08, 0x0B, 0x09, 0x06},
    /* H */ {0x09, 0x09, 0x0F, 0x09, 0x09, 0x09},
    /* I */ {0x07, 0x02, 0x02, 0x02, 0x02, 0x07},
    /* J */ {0x07, 0x01, 0x01, 0x01, 0x09, 0x06},
    /* K */ {0x09, 0x0A, 0x0C, 0x0C, 0x0A, 0x09},
    /* L */ {0x08, 0x08, 0x08, 0x08, 0x08, 0x0F},
    /* M */ {0x09, 0x0F, 0x0F, 0x09, 0x09, 0x09},
    /* N */ {0x09, 0x0D, 0x0D, 0x0B, 0x0B, 0x09},
    /* O */ {0x06, 0x09, 0x09, 0x09, 0x09, 0x06},
    /* P */ {0x0E, 0x09, 0x09, 0x0E, 0x08, 0x08},
    /* Q */ {0x06, 0x09, 0x09, 0x0B, 0x09, 0x06},
    /* R */ {0x0E, 0x09, 0x09, 0x0E, 0x0A, 0x09},
    /* S */ {0x06, 0x09, 0x04, 0x02, 0x09, 0x06},
    /* T */ {0x0F, 0x02, 0x02, 0x02, 0x02, 0x02},
    /* U */ {0x09, 0x09, 0x09, 0x09, 0x09, 0x06},
    /* V */ {0x09, 0x09, 0x09, 0x09, 0x06, 0x06},
    /* W */ {0x09, 0x09, 0x09, 0x0F, 0x0F, 0x09},
    /* X */ {0x09, 0x09, 0x06, 0x06, 0x09, 0x09},
    /* Y */ {0x09, 0x09, 0x06, 0x02, 0x02, 0x02},
    /* Z */ {0x0F, 0x01, 0x02, 0x04, 0x08, 0x0F},
    /* : */ {0x00, 0x02, 0x00, 0x00, 0x02, 0x00},
    /* - */ {0x00, 0x00, 0x0F, 0x00, 0x00, 0x00},
    /* ! */ {0x02, 0x02, 0x02, 0x02, 0x00, 0x02},
    /*   */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* . */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
};

static uint8_t font_index(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 10 + (ch - 'a');
    if (ch == ':') return 36;
    if (ch == '-') return 37;
    if (ch == '!') return 38;
    if (ch == '.') return 40;
    return 39; /* space */
}

static void draw_char(uint8_t *page, uint16_t x, uint8_t y,
                      char ch) {
    uint8_t idx = font_index(ch);
    uint8_t r, c;
    for (r = 0; r < 6; r++) {
        uint8_t bits = font_4x6[idx][r];
        for (c = 0; c < 4; c++) {
            if (bits & (0x08 >> c))
                plot_pixel(page, x + c, y + r);
        }
    }
}

static void draw_string(uint8_t *page, uint16_t x, uint8_t y,
                        const char *str) {
    while (*str) {
        draw_char(page, x, y, *str);
        x += 5;
        str++;
    }
}

/* Center a string: x = (280 - len*5) / 2 */
static void draw_centered(uint8_t *page, uint8_t y, const char *str) {
    uint16_t len = 0;
    const char *p = str;
    while (*p++) len++;
    draw_string(page, (280 - len * 5) / 2, y, str);
}

/* ===================================================================
 * Public API
 * =================================================================== */

void render_init(void) {
    uint8_t i;
    for (i = 0; i < MAX_SLOTS; i++)
        slots[i].active = 0;
    hud_drawn = 0;
}

void render_room(void) {
    uint8_t i, r, c;
    uint8_t start_r, start_c;

    /* Clear background page */
    hgr_clear_page(HGR_PAGE2);

    /* Initialize grid: all cells solid */
    for (r = 0; r < GRID_H; r++)
        for (c = 0; c < GRID_W; c++)
            cave_grid[r][c] = CELL_SOLID;

    /* Trace cave polylines onto grid */
    trace_cave();

    /* Flood fill from player start position to carve interior */
    start_r = grid_row(player_y);
    start_c = grid_col(player_x);
    flood_fill(start_r, start_c);

    /* Render grid cells to background page */
    render_cave_cells(HGR_PAGE2);

    /* Draw cave polylines on top for crisp edges */
    render_cave_lines(HGR_PAGE2);

    /* Draw destroyable walls and lava */
    render_walls_bg(HGR_PAGE2);
    render_lava_bg(HGR_PAGE2);

    /* Copy background to display */
    hgr_copy_page(HGR_PAGE2, HGR_PAGE1);

    /* Reset save-behind and HUD state */
    hud_drawn = 0;
    for (i = 0; i < MAX_SLOTS; i++) {
        slot_drawn[i] = 0;
        slots[i].active = 0;
    }
}

/* Convert game coords to screen + half-height in pixels */
#define HH_PX(hh) (((int16_t)(hh) * 51) >> 5)

void render_frame(void) {
    uint8_t i;
    uint16_t sx;
    uint8_t sy;
    const Sprite *spr;
    uint8_t flip;

    /* Player */
    if (!(game_state == STATE_DYING && !(death_timer & 1))) {
        sx = SCREEN_X(player_x) - 5;
        sy = (uint8_t)(SCREEN_Y(player_y) + HH_PX(PLAYER_HH) - 20);
        flip = (player_facing < 0) ? 1 : 0;
        if (!player_on_ground || player_thrusting) {
            static const Sprite *fly_frames[5] = {
                &spr_player_fly_0, &spr_player_fly_1, &spr_player_fly_2,
                &spr_player_fly_1, &spr_player_fly_0
            };
            spr = fly_frames[(anim_tick / 3) % 5];
        }
        else if (player_vx != 0 && (anim_tick & 4))
            spr = &spr_player_walk_1;
        else
            spr = &spr_player_walk_0;
        erase_draw(0, spr, sx, sy, flip);
    } else {
        restore_behind(0);
    }

    /* Enemies */
    for (i = 0; i < 3; i++) {
        if (i < enemy_count && enemies[i].alive) {
            sx = SCREEN_X(enemies[i].x);
            sy = (uint8_t)SCREEN_Y(enemies[i].y);

            if (enemies[i].type == ENEMY_SPIDER) {
                /* Erase spider area, draw thread + sprite */
                restore_behind(1 + i);
                spr = &spr_spider_0;
                /* Draw thread line */
                hgr_vline(HGR_PAGE1, sx,
                          (uint8_t)SCREEN_Y(enemies[i].home_y), sy);
                blit_sprite_screen(spr, sx - 5,
                    (uint8_t)(sy + HH_PX(SPIDER_HH) - spr->h),
                    1 + i, 0);
            } else if (enemies[i].type == ENEMY_SNAKE) {
                spr = &spr_snake_0;
                flip = (enemies[i].vx < 0) ? 1 : 0;
                erase_draw(1 + i, spr, sx - 7,
                    (uint8_t)(sy + HH_PX(SNAKE_HH) - spr->h), flip);
            } else {
                spr = (enemies[i].anim & 4) ? &spr_bat_0 : &spr_bat_1;
                erase_draw(1 + i, spr, sx - 6,
                    (uint8_t)(sy + HH_PX(BAT_HH) - spr->h), 0);
            }
        } else {
            restore_behind(1 + i);
            slot_drawn[1 + i] = 0;
        }
    }

    /* Miner -- static, draw once */
    if (cur_has_miner && !slots[4].active) {
        blit_sprite_screen(&spr_miner_0,
            SCREEN_X(cur_miner_x) - 5,
            (uint8_t)(SCREEN_Y(cur_miner_y) + HH_PX(MINER_HH) - spr_miner_0.h),
            4, 0);
    }

    /* Dynamite / Explosion */
    if (dyn_active) {
        if (dyn_exploding) {
            restore_behind(5);
            /* Draw explosion as expanding cross */
            {
                uint16_t ex = SCREEN_X(dyn_x);
                uint8_t ey = (uint8_t)SCREEN_Y(dyn_y);
                uint8_t r = (uint8_t)((EXPLOSION_TIME - dyn_expl_timer) * 6);
                if (r > 30) r = 30;
                hgr_hline(HGR_PAGE1, ex > r ? ex - r : 0, ex + r, ey);
                hgr_vline(HGR_PAGE1, ex, ey > r ? ey - r : 0, ey + r);
                mark_behind(5, (uint8_t)((ex > r ? ex - r : 0) / 7),
                           ey > r ? ey - r : 0,
                           (uint8_t)(r / 3 + 2),
                           (uint8_t)(r * 2 + 1));
            }
        } else {
            restore_behind(5);
            blit_sprite_screen(&spr_dynamite_0,
                SCREEN_X(dyn_x) - 2,
                (uint8_t)(SCREEN_Y(dyn_y) - 5), 5, 0);
        }
    } else {
        restore_behind(5);
        slot_drawn[5] = 0;
    }

    /* Laser */
    if (laser_active) {
        uint16_t lx1 = SCREEN_X(laser_x);
        uint16_t lx2 = SCREEN_X(laser_x + laser_dir * LASER_LENGTH);
        uint8_t ly = (uint8_t)SCREEN_Y(laser_y);
        if (lx1 > lx2) { uint16_t t = lx1; lx1 = lx2; lx2 = t; }
        mark_behind(6, (uint8_t)(lx1 / 7), ly,
                   (uint8_t)((lx2 - lx1) / 7 + 2), 1);
        hgr_hline(HGR_PAGE1, lx1, lx2, ly);
    } else if (slots[6].active) {
        restore_behind(6);
        slot_drawn[6] = 0;
    }
}

void render_hud(void) {
    char buf[8];
    uint8_t i;

#define HUD_TEXT_Y    5
#define FUEL_BAR_W    100
#define FUEL_BAR_X    90
#define FUEL_BAR_ROW  180

    if (!hud_drawn) {
        /* Clear HUD area on display page */
        {
            uint8_t y;
            for (y = 0; y < HUD_HEIGHT; y++) {
                uint16_t row_off = hgr_row_addr[y];
                uint8_t *row = HGR_PAGE1 + row_off;
                uint8_t b;
                for (b = 0; b < SCREEN_STRIDE; b++) row[b] = 0;
            }
        }

        draw_string(HGR_PAGE1, 2, HUD_TEXT_Y, "LV");
        buf[0] = '0' + ((current_level + 1) / 10);
        buf[1] = '0' + ((current_level + 1) % 10);
        buf[2] = 0;
        draw_string(HGR_PAGE1, 14, HUD_TEXT_Y, buf);

        draw_string(HGR_PAGE1, 200, HUD_TEXT_Y, "SCORE");

        /* Draw POWER label */
        draw_string(HGR_PAGE1, 50, FUEL_BAR_ROW, "POWER");

        hud_last_score = -1;
        hud_last_fuel = 254;
        hud_last_lives = 255;
        hud_last_dyn = 255;
        hud_drawn = 1;
    }

    /* Lives */
    if (player_lives != hud_last_lives) {
        uint16_t lx = 50;
        /* Clear lives area */
        for (i = 0; i < START_LIVES; i++)
            draw_string(HGR_PAGE1, lx + i * 10, HUD_TEXT_Y, " ");
        for (i = 0; i < player_lives; i++)
            draw_char(HGR_PAGE1, lx + i * 10, HUD_TEXT_Y, 'H');
        hud_last_lives = player_lives;
    }

    /* Dynamite count */
    if (player_dynamite != hud_last_dyn) {
        uint16_t dx = 110;
        for (i = 0; i < START_DYNAMITE; i++)
            draw_string(HGR_PAGE1, dx + i * 10, HUD_TEXT_Y, " ");
        for (i = 0; i < player_dynamite; i++)
            draw_char(HGR_PAGE1, dx + i * 10, HUD_TEXT_Y, 'D');
        hud_last_dyn = player_dynamite;
    }

    /* Score */
    if (score != hud_last_score) {
        int16_t s = score;
        uint8_t d;
        for (d = 0; d < 5; d++) {
            buf[4 - d] = '0' + (s % 10);
            s /= 10;
        }
        buf[5] = 0;
        draw_string(HGR_PAGE1, 230, HUD_TEXT_Y, buf);
        hud_last_score = score;
    }

    /* Fuel bar */
    if (player_fuel != hud_last_fuel) {
        uint8_t new_filled = (uint8_t)((uint16_t)player_fuel * FUEL_BAR_W / START_FUEL);
        uint16_t row_off = hgr_row_addr[FUEL_BAR_ROW];
        uint8_t *row = HGR_PAGE1 + row_off;
        uint8_t start_byte = (uint8_t)(FUEL_BAR_X / 7);
        uint8_t fill_bytes = (uint8_t)(new_filled / 7);
        uint8_t b;
        /* Fill fuel bar */
        for (b = 0; b < fill_bytes && (start_byte + b) < SCREEN_STRIDE; b++)
            row[start_byte + b] = 0x7F;
        /* Clear remainder */
        for (; b < FUEL_BAR_W / 7 && (start_byte + b) < SCREEN_STRIDE; b++)
            row[start_byte + b] = 0x00;
        hud_last_fuel = player_fuel;
    }
}

void render_destroy_wall(uint8_t idx) {
    uint16_t wx, ww;
    uint8_t wy, wh, r;

    wx = SCREEN_X(wall_x(idx));
    wy = (uint8_t)SCREEN_Y(wall_y(idx) + wall_h(idx));
    ww = SCREEN_X(wall_x(idx) + wall_w(idx)) - wx;
    wh = (uint8_t)(SCREEN_Y(wall_y(idx) - wall_h(idx)) - wy);
    if (wh < 2) wh = 2;

    /* Clear wall on both pages */
    for (r = 0; r < wh && (wy + r) < SCREEN_H; r++) {
        uint16_t row_off = hgr_row_addr[wy + r];
        uint8_t start_byte = (uint8_t)(wx / 7);
        uint8_t end_byte = (uint8_t)((wx + ww) / 7);
        uint8_t b;
        for (b = start_byte; b <= end_byte && b < SCREEN_STRIDE; b++) {
            HGR_PAGE1[row_off + b] = 0;
            HGR_PAGE2[row_off + b] = 0;
        }
    }
}

void render_title_screen(void) {
    hgr_clear_page(HGR_PAGE1);
    draw_centered(HGR_PAGE1, 50, "R.E.S.C.U.E.");
    draw_centered(HGR_PAGE1, 70, "APPLE II");
    draw_centered(HGR_PAGE1, 100, "I:UP  J:LEFT  K:DOWN  L:RIGHT");
    draw_centered(HGR_PAGE1, 115, "SPACE:LASER  Z:DYNAMITE");
    draw_centered(HGR_PAGE1, 140, "PRESS RETURN TO START");
}

void render_level_intro(void) {
    char buf[12];
    hgr_clear_page(HGR_PAGE1);
    buf[0] = 'L'; buf[1] = 'E'; buf[2] = 'V'; buf[3] = 'E'; buf[4] = 'L'; buf[5] = ' ';
    buf[6] = '0' + ((current_level + 1) / 10);
    buf[7] = '0' + ((current_level + 1) % 10);
    buf[8] = 0;
    draw_centered(HGR_PAGE1, 90, buf);
}

void render_game_over(void) {
    hgr_clear_page(HGR_PAGE1);
    draw_centered(HGR_PAGE1, 80, "GAME OVER");
    draw_centered(HGR_PAGE1, 110, "PRESS RETURN");
}

void render_rescued(void) {
    hgr_clear_page(HGR_PAGE1);
    draw_centered(HGR_PAGE1, 80, "RESCUED!");
    draw_centered(HGR_PAGE1, 110, "PRESS RETURN");
}

void render_failed(void) {
    hgr_clear_page(HGR_PAGE1);
    draw_centered(HGR_PAGE1, 80, "FAILED");
    draw_centered(HGR_PAGE1, 110, "PRESS RETURN");
}
