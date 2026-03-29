/*
 * render.c — Sinclair QL Mode 8 rendering engine
 *
 * Background-restore approach:
 *   - Cave geometry rasterized once per room into bg_buffer
 *   - Each frame: restore sprite rects from bg_buffer, draw sprites
 *   - HUD updated incrementally
 */

#include "game.h"
#include "sprites.h"

/* Background buffer — allocated at runtime via QDOS MT.ALCHP */
static uint8_t *bg_buffer;

/* HUD dirty tracking */
static int16_t hud_last_score;
static uint8_t hud_last_fuel;
static uint8_t hud_last_lives;
static uint8_t hud_last_dyn;
static uint8_t hud_drawn;

/* ===================================================================
 * Cave grid for rasterization (64x60 cells, each = 4x4 pixels)
 * =================================================================== */

#define GRID_W  64
#define GRID_H  60
#define CELL_W  4
#define CELL_H  4

#define CELL_SOLID  2
#define CELL_BORDER 1
#define CELL_EMPTY  0

static uint8_t cave_grid[GRID_H][GRID_W];

/* ===================================================================
 * Save-behind slots for sprite erasing
 * =================================================================== */

#define MAX_SLOTS   8
#define MAX_SAVE_W  8   /* max bytes per row (16 pixels) */
#define MAX_SAVE_H  24  /* max pixel rows */

typedef struct {
    int16_t x, y;
    uint8_t wb, h;  /* width in bytes, height in pixels */
    uint8_t active;
    /* Track last drawn position to skip redundant redraws */
    int16_t last_sx, last_sy;
    const uint8_t *last_spr;  /* last sprite data pointer */
    uint8_t data[MAX_SAVE_W * MAX_SAVE_H];
} SaveSlot;

static SaveSlot slots[MAX_SLOTS];

/* ===================================================================
 * Mode 8 pixel primitives
 *
 * QL Mode 8 screen: 256x256, 128 bytes/row, bytes in pairs:
 *   even byte = green plane, odd byte = red/flash plane
 *   each pair = 4 pixels (2 bits per pixel per plane)
 *
 * Pixel N (0-3) in byte pair at addr (even), addr+1 (odd):
 *   G = bit (7-2N) of even byte     (green component)
 *   S = bit (6-2N) of even byte     (stipple, keep 0)
 *   B = bit (7-2N) of odd byte      (blue/flash)
 *   R = bit (6-2N) of odd byte      (red component)
 *   Color = G*4 + R*2 + B
 *
 * Pre-computed byte values for all-4-pixels-same-color:
 * =================================================================== */

/* Even (green) and odd (red/flash) byte for 4 identical pixels */
static const uint8_t col_even[8] = {
    0x00, 0x00, 0x00, 0x00, 0xAA, 0xAA, 0xAA, 0xAA
};
static const uint8_t col_odd[8] = {
    0x00, 0xAA, 0x55, 0xFF, 0x00, 0xAA, 0x55, 0xFF
};

/* Bit masks for individual pixel positions within a byte */
static const uint8_t pix_mask[4]   = { 0xC0, 0x30, 0x0C, 0x03 };
static const uint8_t pix_clear[4]  = { 0x3F, 0xCF, 0xF3, 0xFC };

static void plot_pixel(uint8_t *base, int16_t x, int16_t y, uint8_t color) {
    uint8_t *even_addr, *odd_addr;
    uint8_t pos, g_bits, r_bits, mask, clear;
    if ((uint16_t)x >= SCREEN_W || (uint16_t)y >= SCREEN_H) return;

    /* Byte pair address: 4 pixels per pair, pairs at even addresses */
    even_addr = base + (uint16_t)y * SCREEN_STRIDE + ((x >> 2) << 1);
    odd_addr = even_addr + 1;
    pos = x & 3;  /* pixel position 0-3 within the pair */
    clear = pix_clear[pos];

    /* Green component: bit 2 of color → G position */
    g_bits = (color & 4) ? pix_mask[pos] & 0xAA : 0;  /* high bit of pair */
    /* Blue/Flash: bit 0 of color → B position */
    r_bits = (color & 1) ? pix_mask[pos] & 0xAA : 0;  /* high bit of pair */
    /* Red: bit 1 of color → R position */
    r_bits |= (color & 2) ? pix_mask[pos] & 0x55 : 0; /* low bit of pair */

    *even_addr = (*even_addr & clear) | g_bits;
    *odd_addr  = (*odd_addr  & clear) | r_bits;
}

static void hline(uint8_t *base, int16_t x1, int16_t x2, int16_t y, uint8_t color) {
    int16_t x;
    uint8_t *row;
    uint8_t ev, od;
    if ((uint16_t)y >= SCREEN_H) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= SCREEN_W) x2 = SCREEN_W - 1;
    if (x1 > x2) return;
    row = base + (uint16_t)y * SCREEN_STRIDE;
    ev = col_even[color & 7];
    od = col_odd[color & 7];

    /* Write individual pixels until aligned to 4-pixel boundary */
    while ((x1 & 3) && x1 <= x2) { plot_pixel(base, x1, y, color); x1++; }
    /* Write individual pixels at the end */
    while ((x2 & 3) != 3 && x2 >= x1) { plot_pixel(base, x2, y, color); x2--; }
    /* Fill full byte pairs (4 pixels each) */
    for (x = x1; x <= x2; x += 4) {
        uint16_t pair_addr = (x >> 2) << 1;
        row[pair_addr]     = ev;
        row[pair_addr + 1] = od;
    }
}

static void filled_rect(uint8_t *base, int16_t x, int16_t y,
                        int16_t w, int16_t h, uint8_t color) {
    int16_t r;
    for (r = y; r < y + h; r++)
        hline(base, x, x + w - 1, r, color);
}

/* Bresenham line drawing */
static void draw_line(uint8_t *base, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint8_t color) {
    int16_t dx, dy, sx, sy, err, e2;
    dx = x2 - x1; if (dx < 0) dx = -dx;
    dy = y2 - y1; if (dy < 0) dy = -dy;
    sx = x1 < x2 ? 1 : -1;
    sy = y1 < y2 ? 1 : -1;
    err = dx - dy;

    for (;;) {
        plot_pixel(base, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

/* ===================================================================
 * Cave rasterization
 * =================================================================== */

/* Map game coords to grid cell */
static uint8_t grid_col(int8_t gx) {
    return (uint8_t)(((uint16_t)((uint8_t)(gx + 128)) * GRID_W) >> 8);
}
static uint8_t grid_row(int8_t gy) {
    return (uint8_t)(((uint16_t)(50 - gy) * (GRID_H - 1)) / 100);
}

/* Trace a line onto the cell grid */
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

/* Trace all polylines */
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

/* Fast stack-based flood fill (replaces slow iterative multi-pass) */
#define FILL_STACK_SIZE 512
static uint16_t fill_stack[FILL_STACK_SIZE]; /* packed: (r<<8)|c */
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

/* Render grid cells to background buffer — FAST version.
 * Each cell is 4x4 pixels = exactly 1 byte-pair (2 bytes) x 4 rows.
 * Write byte-pairs directly instead of using plot_pixel. */
static void render_cave_cells(void) {
    uint8_t r, c, cell;
    uint8_t ev, od;
    uint8_t *base;
    uint16_t row_addr;
    uint8_t y;

    for (r = 0; r < GRID_H; r++) {
        row_addr = (uint16_t)(HUD_HEIGHT + r * CELL_H) * SCREEN_STRIDE;
        for (c = 0; c < GRID_W; c++) {
            cell = cave_grid[r][c];
            if (cell == CELL_EMPTY) continue;  /* black, already cleared */

            if (cell == CELL_BORDER) {
                ev = col_even[COL_WHITE];  /* 0xAA */
                od = col_odd[COL_WHITE];   /* 0xFF */
            } else {
                /* Rock: green */
                ev = col_even[COL_GREEN];  /* 0xAA */
                od = col_odd[COL_GREEN];   /* 0x00 */
            }

            /* Each cell = 4 pixels wide = 1 byte-pair, 4 rows tall */
            base = bg_buffer + row_addr + (c << 1);  /* c*2 for byte-pair */
            for (y = 0; y < CELL_H; y++) {
                base[0] = ev;
                base[1] = od;
                base += SCREEN_STRIDE;
            }
        }
    }
}

/* Draw polylines on top of cells for sharp borders */
static void render_cave_lines(void) {
    const int8_t *p = cur_cave_lines;
    uint8_t n, i;
    int8_t cx, cy;

    while ((n = (uint8_t)*p++) != 0) {
        cy = p[0]; cx = p[1]; p += 2;
        for (i = 0; i < n; i++) {
            int8_t ny = cy + p[0];
            int8_t nx = cx + p[1];
            draw_line(bg_buffer,
                      SCREEN_X(cx), SCREEN_Y(cy),
                      SCREEN_X(nx), SCREEN_Y(ny), COL_WHITE);
            cy = ny; cx = nx;
            p += 2;
        }
    }
}

/* Draw destroyable walls */
static void render_walls_bg(void) {
    uint8_t i;
    for (i = 0; i < cur_wall_count; i++) {
        int16_t wx, wy, ww, wh;
        if (walls_destroyed & (1 << i)) continue;
        wx = SCREEN_X(wall_x(i));
        wy = SCREEN_Y(wall_y(i) + wall_h(i));
        ww = SCREEN_X(wall_x(i) + wall_w(i)) - wx;
        wh = SCREEN_Y(wall_y(i) - wall_h(i)) - wy;
        if (ww < 2) ww = 2;
        if (wh < 2) wh = 2;
        filled_rect(bg_buffer, wx, wy, ww, wh, COL_YELLOW);
    }
}

/* Draw lava strip */
static void render_lava_bg(void) {
    int16_t lx1, lx2, ly;
    if (!cur_has_lava) return;
    lx1 = SCREEN_X(cur_cave_left);
    lx2 = SCREEN_X(cur_cave_right);
    ly = SCREEN_Y(cur_cave_floor);
    hline(bg_buffer, lx1, lx2, ly,     COL_RED);
    hline(bg_buffer, lx1, lx2, ly + 1, COL_RED);
    hline(bg_buffer, lx1, lx2, ly + 2, COL_MAGENTA);
    hline(bg_buffer, lx1, lx2, ly + 3, COL_RED);
}

/* ===================================================================
 * Sprite blitting with save-behind
 * =================================================================== */

/* Save/restore use raw byte offsets into screen memory.
 * Restore from bg_buffer instead of saved data — avoids separate save step. */
static void restore_behind(uint8_t slot) {
    SaveSlot *s = &slots[slot];
    uint8_t *src, *dst;
    uint8_t r, c;
    if (!s->active) return;
    for (r = 0; r < s->h; r++) {
        if ((uint16_t)(s->y + r) >= SCREEN_H) break;
        dst = SCREEN_BASE + (uint16_t)(s->y + r) * SCREEN_STRIDE + s->x;
        src = bg_buffer   + (uint16_t)(s->y + r) * SCREEN_STRIDE + s->x;
        for (c = 0; c < s->wb; c++)
            dst[c] = src[c];
    }
    s->active = 0;
}

/* Mark area for restore (no need to save — we restore from bg_buffer) */
static void mark_behind(uint8_t slot, int16_t x, int16_t y,
                        uint8_t wb, uint8_t h) {
    SaveSlot *s = &slots[slot];
    s->x = x; s->y = y; s->wb = wb; s->h = h; s->active = 1;
}

/*
 * Draw sprite with transparency (color 0 = skip).
 * Sprite data is nibble-packed: 2 pixels per byte (high nibble = left).
 * Uses plot_pixel for correct Mode 8 bit-plane output.
 */
static void blit_sprite(const Sprite *spr, int16_t sx, int16_t sy,
                        uint8_t slot, uint8_t flip_h) {
    uint8_t wb, r, c, px;
    const uint8_t *src;
    int16_t save_x, save_wb;

    wb = spr->w >> 1;  /* bytes per row in sprite data */

    /* Save behind (save the screen byte-pairs under the sprite) */
    save_x = (sx >> 2) << 1;  /* align to byte-pair boundary */
    save_wb = ((spr->w + 3) >> 2) << 1;  /* byte-pairs needed */
    if (save_wb > MAX_SAVE_W) save_wb = MAX_SAVE_W;
    if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy + spr->h <= SCREEN_H) {
        mark_behind(slot, save_x, sy, (uint8_t)save_wb, spr->h);
    }

    /* Blit pixel by pixel */
    for (r = 0; r < spr->h; r++) {
        int16_t py = sy + r;
        if ((uint16_t)py >= SCREEN_H) continue;
        src = spr->data + r * wb;
        for (c = 0; c < wb; c++) {
            uint8_t byte;
            uint8_t hi, lo;
            int16_t pixel_x;

            if (flip_h)
                byte = src[wb - 1 - c];
            else
                byte = src[c];

            if (flip_h) {
                hi = byte & 0x0F;  /* swap nibbles for flip */
                lo = (byte >> 4) & 0x0F;
            } else {
                hi = (byte >> 4) & 0x0F;
                lo = byte & 0x0F;
            }

            pixel_x = sx + c * 2;
            if (hi) plot_pixel(SCREEN_BASE, pixel_x, py, hi);
            if (lo) plot_pixel(SCREEN_BASE, pixel_x + 1, py, lo);
        }
    }
}

/* ===================================================================
 * Simple 4x6 bitmap font for HUD
 * =================================================================== */

static const uint8_t font_4x6[40][6] = {
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
};

static uint8_t font_index(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 10 + (ch - 'a');
    if (ch == ':') return 36;
    if (ch == '-') return 37;
    if (ch == '!') return 38;
    return 39; /* space */
}

static void draw_char(uint8_t *base, int16_t x, int16_t y,
                      char ch, uint8_t color) {
    uint8_t idx = font_index(ch);
    uint8_t r, c;
    for (r = 0; r < 6; r++) {
        uint8_t bits = font_4x6[idx][r];
        for (c = 0; c < 4; c++) {
            if (bits & (0x08 >> c))
                plot_pixel(base, x + c, y + r, color);
        }
    }
}

static void draw_string(uint8_t *base, int16_t x, int16_t y,
                        const char *str, uint8_t color) {
    while (*str) {
        draw_char(base, x, y, *str, color);
        x += 5;
        str++;
    }
}

/* ===================================================================
 * Copy background buffer to screen
 * =================================================================== */

static void copy_bg_to_screen(void) {
    uint32_t *src = (uint32_t *)bg_buffer;
    uint32_t *dst = (uint32_t *)SCREEN_BASE;
    uint16_t i;
    /* Copy 32KB as longwords — 6x faster than bytes on 68008 */
    for (i = 0; i < SCREEN_SIZE / 4; i++)
        *dst++ = *src++;
}

/* ===================================================================
 * Public API
 * =================================================================== */

void render_init(void) {
    uint8_t i;
    for (i = 0; i < MAX_SLOTS; i++)
        slots[i].active = 0;
    /* Allocate background buffer from QDOS common heap */
    bg_buffer = (uint8_t *)ql_alloc((uint32_t)SCREEN_SIZE);
}

void render_room(void) {
    uint8_t i;

    /* Clear background buffer (longword for speed) */
    {
        uint32_t *p = (uint32_t *)bg_buffer;
        uint16_t j;
        for (j = 0; j < SCREEN_SIZE / 4; j++) p[j] = 0;
    }

    /* Draw cave polylines directly — no grid, no flood fill.
     * Black background with green/white wall lines, like the Vectrex original. */
    render_cave_lines();
    render_walls_bg();
    render_lava_bg();

    copy_bg_to_screen();

    /* Reset save-behind slots and HUD */
    hud_drawn = 0;
    for (i = 0; i < MAX_SLOTS; i++)
        slots[i].active = 0;
}

/* HUD dirty tracking (forward declared for render_room) */

void render_hud(void) {
    char buf[8];
    int16_t i, filled;

    if (!hud_drawn) {
        /* First draw: full HUD */
        filled_rect(SCREEN_BASE, 0, 0, SCREEN_W, HUD_HEIGHT, COL_BLACK);
        draw_string(SCREEN_BASE, 2, 2, "L", COL_WHITE);
        buf[0] = '0' + ((current_level + 1) / 10);
        buf[1] = '0' + ((current_level + 1) % 10);
        buf[2] = 0;
        draw_string(SCREEN_BASE, 7, 2, buf, COL_WHITE);
        hud_last_score = -1;
        hud_last_fuel = 255;
        hud_last_lives = 255;
        hud_last_dyn = 255;
        hud_drawn = 1;
    }

    /* Lives — only if changed */
    if (player_lives != hud_last_lives) {
        for (i = 0; i < START_LIVES; i++) {
            uint8_t color = (i < player_lives) ? COL_RED : COL_BLACK;
            draw_char(SCREEN_BASE, 25 + i * 6, 2, 'O', color);
        }
        hud_last_lives = player_lives;
    }

    /* Dynamite — only if changed */
    if (player_dynamite != hud_last_dyn) {
        for (i = 0; i < START_DYNAMITE; i++) {
            uint8_t color = (i < player_dynamite) ? COL_YELLOW : COL_BLACK;
            draw_char(SCREEN_BASE, 50 + i * 6, 2, '!', color);
        }
        hud_last_dyn = player_dynamite;
    }

    /* Score — only if changed */
    if (score != hud_last_score) {
        int16_t s = score;
        uint8_t d;
        for (d = 0; d < 5; d++) {
            buf[4 - d] = '0' + (s % 10);
            s /= 10;
        }
        buf[5] = 0;
        draw_string(SCREEN_BASE, 200, 2, buf, COL_WHITE);
        hud_last_score = score;
    }

    /* Fuel bar — only if changed */
    if (player_fuel != hud_last_fuel) {
        filled = (int16_t)((uint16_t)player_fuel * 200 / START_FUEL);
        hline(SCREEN_BASE, 20, 220, 10, COL_BLACK);
        hline(SCREEN_BASE, 20, 220, 11, COL_BLACK);
        if (filled > 0) {
            hline(SCREEN_BASE, 20, 20 + filled, 10,
                  filled > 50 ? COL_GREEN : COL_RED);
            hline(SCREEN_BASE, 20, 20 + filled, 11,
                  filled > 50 ? COL_GREEN : COL_RED);
        }
        hud_last_fuel = player_fuel;
    }
}

/* Helper: erase sprite slot, then draw sprite at new position.
 * Skips entirely if position and sprite haven't changed. */
static void erase_draw(uint8_t slot, const Sprite *spr,
                       int16_t sx, int16_t sy, uint8_t flip) {
    SaveSlot *s = &slots[slot];
    /* Skip if nothing changed */
    if (s->active && sx == s->last_sx && sy == s->last_sy
        && spr->data == s->last_spr)
        return;
    restore_behind(slot);
    blit_sprite(spr, sx, sy, slot, flip);
    s->last_sx = sx;
    s->last_sy = sy;
    s->last_spr = spr->data;
}

void render_frame(void) {
    uint8_t i;
    int16_t sx, sy;
    const Sprite *spr;
    uint8_t flip;

    /*
     * Sprite positioning: align sprite bottom with collision box bottom.
     * Game HH in screen pixels: hh_px = (hh * 77) >> 5
     * Sprite Y = SCREEN_Y(game_y) + hh_px - sprite_height
     * Sprite X = SCREEN_X(game_x) - sprite_width/2
     */
#define HH_PX(hh) (((int16_t)(hh) * 77) >> 5)

    /* Player */
    if (!(game_state == STATE_DYING && !(death_timer & 2))) {
        sx = SCREEN_X(player_x) - 5;
        sy = SCREEN_Y(player_y) + HH_PX(PLAYER_HH) - 20;
        flip = (player_facing < 0) ? 1 : 0;
        if (!player_on_ground || player_thrusting)
            spr = &spr_player_fly;
        else if (player_vx != 0 && (anim_tick & 8))
            spr = &spr_player_walk;
        else
            spr = &spr_player_r;
        erase_draw(0, spr, sx, sy, flip);
    } else {
        restore_behind(0);
    }

    /* Enemies */
    for (i = 0; i < 3; i++) {
        if (i < enemy_count && enemies[i].alive) {
            sx = SCREEN_X(enemies[i].x);
            sy = SCREEN_Y(enemies[i].y);

            if (enemies[i].type == ENEMY_SPIDER) {
                restore_behind(1 + i);
                draw_line(SCREEN_BASE, sx, SCREEN_Y(enemies[i].home_y),
                          sx, sy, COL_WHITE);
                spr = &spr_spider;
                blit_sprite(spr, sx - 5,
                            sy + HH_PX(SPIDER_HH) - spr->h, 1 + i, 0);
            } else if (enemies[i].type == ENEMY_SNAKE) {
                spr = &spr_snake_r;
                flip = (enemies[i].vx < 0) ? 1 : 0;
                erase_draw(1 + i, spr, sx - 7,
                           sy + HH_PX(SNAKE_HH) - spr->h, flip);
            } else {
                spr = (enemies[i].anim & 8) ? &spr_bat0 : &spr_bat1;
                erase_draw(1 + i, spr, sx - 6,
                           sy + HH_PX(BAT_HH) - spr->h, 0);
            }
        } else {
            restore_behind(1 + i);
        }
    }

    /* Miner — static, only draw once */
    if (cur_has_miner && !slots[4].active) {
        blit_sprite(&spr_miner,
                    SCREEN_X(cur_miner_x) - 5,
                    SCREEN_Y(cur_miner_y) + HH_PX(MINER_HH) - spr_miner.h,
                    4, 0);
    }

    /* Dynamite / Explosion */
    if (dyn_active) {
        if (dyn_exploding)
            erase_draw(5, &spr_explode,
                       SCREEN_X(dyn_x) - 6,
                       SCREEN_Y(dyn_y) - 6, 0);
        else
            erase_draw(5, &spr_dynamite,
                       SCREEN_X(dyn_x) - 2,
                       SCREEN_Y(dyn_y) - 5, 0);
    } else {
        restore_behind(5);
    }

    /* Laser */
    if (laser_active) {
        int8_t lx;
        int16_t step = LASER_LENGTH / 3;
        for (i = 0; i < 3; i++) {
            lx = laser_x + laser_dir * (int8_t)(step * i);
            erase_draw(6 + i, &spr_laser,
                       SCREEN_X(lx), SCREEN_Y(laser_y) - 2, 0);
        }
    }
}

void render_destroy_wall(uint8_t idx) {
    int16_t wx, wy, ww, wh;
    wx = SCREEN_X(wall_x(idx));
    wy = SCREEN_Y(wall_y(idx) + wall_h(idx));
    ww = SCREEN_X(wall_x(idx) + wall_w(idx)) - wx;
    wh = SCREEN_Y(wall_y(idx) - wall_h(idx)) - wy;
    if (ww < 2) ww = 2;
    if (wh < 2) wh = 2;
    /* Clear on screen and in background buffer */
    filled_rect(SCREEN_BASE, wx, wy, ww, wh, COL_BLACK);
    filled_rect(bg_buffer, wx, wy, ww, wh, COL_BLACK);
}

/* ===================================================================
 * Full-screen displays
 * =================================================================== */

static void clear_screen(void) {
    uint16_t i;
    for (i = 0; i < SCREEN_SIZE; i++)
        SCREEN_BASE[i] = 0;
}

void render_title_screen(void) {
    clear_screen();
    draw_string(SCREEN_BASE, 72, 80, "H.E.R.O.", COL_YELLOW);
    draw_string(SCREEN_BASE, 52, 100, "SINCLAIR QL", COL_CYAN);
    draw_string(SCREEN_BASE, 42, 140, "Q:UP A:DN O:LT P:RT", COL_WHITE);
    draw_string(SCREEN_BASE, 52, 155, "SPACE:LASER D:DYN", COL_WHITE);
    draw_string(SCREEN_BASE, 52, 185, "PRESS ENTER TO START", COL_GREEN);
}

void render_level_intro(void) {
    char buf[12];
    clear_screen();
    buf[0] = 'L'; buf[1] = 'E'; buf[2] = 'V'; buf[3] = 'E'; buf[4] = 'L'; buf[5] = ' ';
    buf[6] = '0' + ((current_level + 1) / 10);
    buf[7] = '0' + ((current_level + 1) % 10);
    buf[8] = 0;
    draw_string(SCREEN_BASE, 88, 120, buf, COL_YELLOW);
}

void render_game_over(void) {
    clear_screen();
    draw_string(SCREEN_BASE, 72, 100, "GAME OVER", COL_RED);
    draw_string(SCREEN_BASE, 52, 140, "PRESS ENTER", COL_WHITE);
}

void render_rescued(void) {
    clear_screen();
    draw_string(SCREEN_BASE, 72, 100, "RESCUED!", COL_CYAN);
    draw_string(SCREEN_BASE, 52, 140, "PRESS ENTER", COL_WHITE);
}

void render_failed(void) {
    clear_screen();
    draw_string(SCREEN_BASE, 88, 100, "FAILED", COL_RED);
    draw_string(SCREEN_BASE, 52, 140, "PRESS ENTER", COL_WHITE);
}
