//
// render.c — Terminal rendering engine with Unicode line-drawing
//

#include "game.h"
#include <stdio.h>
#include <unistd.h>

// Frame buffer — each cell holds a UTF-8 string (up to 4 bytes)
static char    fb_char[TOTAL_ROWS][SCREEN_W][5];
static uint8_t fb_color[TOTAL_ROWS][SCREEN_W];

// Output buffer (generous for UTF-8 + ANSI codes)
#define OUT_BUF_SIZE (TOTAL_ROWS * SCREEN_W * 24)
static char out_buf[OUT_BUF_SIZE];
static int out_pos;

static void out_str(const char *s) {
    while (*s && out_pos < OUT_BUF_SIZE - 1)
        out_buf[out_pos++] = *s++;
}

// =========================================================================
// Unicode character constants (UTF-8 encoded)
// =========================================================================

// Box-drawing / line characters
#define CH_HORIZ    "\u2500"   // ─
#define CH_VERT     "\u2502"   // │
#define CH_DIAG_FWD "\u2571"   // ╱
#define CH_DIAG_BWD "\u2572"   // ╲
#define CH_CROSS    "\u253C"   // ┼
#define CH_TL       "\u250C"   // ┌
#define CH_TR       "\u2510"   // ┐
#define CH_BL       "\u2514"   // └
#define CH_BR       "\u2518"   // ┘
#define CH_T_DOWN   "\u252C"   // ┬
#define CH_T_UP     "\u2534"   // ┴
#define CH_T_RIGHT  "\u251C"   // ├
#define CH_T_LEFT   "\u2524"   // ┤

// Block characters
#define CH_FULL     "\u2588"   // █
#define CH_DARK     "\u2593"   // ▓
#define CH_MED      "\u2592"   // ▒
#define CH_LIGHT    "\u2591"   // ░

// Game characters
#define CH_PLAYER   "\u25C6"   // ◆
#define CH_MINER    "\u263B"   // ☻
#define CH_HEART    "\u2665"   // ♥
#define CH_BULLET   "\u25CF"   // ●
#define CH_DIAMOND  "\u25C8"   // ◈
#define CH_STAR     "\u2736"   // ✶
#define CH_SPARK    "\u2022"   // •
#define CH_LASER    "\u2550"   // ═
#define CH_LAVA1    "\u2248"   // ≈
#define CH_LAVA2    "\u223C"   // ∼
#define CH_BAT_UP   "\u2227"   // ∧ (wings up)
#define CH_BAT_DN   "\u2228"   // ∨ (wings down)
#define CH_SPIDER   "\u2573"   // ╳
#define CH_SNAKE1   "\u223F"   // ∿
#define CH_SNAKE2   "\u2307"   // ⌇
#define CH_PROP1    "\u2261"   // ≡
#define CH_PROP2    "\u2A2F"   // ⨯
#define CH_ARROW_R  "\u25B6"   // ▶
#define CH_ARROW_L  "\u25C0"   // ◀
#define CH_DYN      "\u2302"   // ⌂
#define CH_FUSE     "\u2020"   // †
#define CH_BAR_FULL "\u25AE"   // ▮
#define CH_BAR_EMPTY "\u25AF"  // ▯

// Player sprite parts
#define CH_HEAD     "\u25CB"   // ○
#define CH_BODY     "\u25A0"   // ■
#define CH_BELT     "\u2501"   // ━
#define CH_FOOT_L   "\u2597"   // ▗
#define CH_FOOT_R   "\u2596"   // ▖
#define CH_LEG_V    "\u2502"   // │ (legs together)
#define CH_LEG_HANG "\u2575"   // ╵ (dangling)
#define CH_STRIDE_L "\u2571"   // ╱
#define CH_STRIDE_R "\u2572"   // ╲
#define CH_FLAME1   "\u2593"   // ▓
#define CH_FLAME2   "\u2591"   // ░
#define CH_ARM_R    "\u2500"   // ─
#define CH_ARM_L    "\u2500"   // ─

// =========================================================================
// Coordinate mapping: game coords (-128..127) -> screen coords
// =========================================================================

static int game_to_col(int gx) {
    return (gx + 128) * SCREEN_W / 256;
}

static int game_to_row(int gy) {
    return HUD_ROWS + (127 - gy) * SCREEN_H / 256;
}

// =========================================================================
// Frame buffer operations
// =========================================================================

void render_clear(void) {
    int r, c;
    for (r = 0; r < TOTAL_ROWS; r++)
        for (c = 0; c < SCREEN_W; c++) {
            fb_char[r][c][0] = ' ';
            fb_char[r][c][1] = '\0';
            fb_color[r][c] = COL_BLACK;
        }
}

// Set a cell to a UTF-8 string
static void render_cell(int row, int col, const char *ch, uint8_t color) {
    if (row >= 0 && row < TOTAL_ROWS && col >= 0 && col < SCREEN_W) {
        int i;
        for (i = 0; i < 4 && ch[i]; i++)
            fb_char[row][col][i] = ch[i];
        fb_char[row][col][i] = '\0';
        fb_color[row][col] = color;
    }
}

// Set a cell to a single ASCII char
void render_char(int row, int col, char ch, uint8_t color) {
    if (row >= 0 && row < TOTAL_ROWS && col >= 0 && col < SCREEN_W) {
        fb_char[row][col][0] = ch;
        fb_char[row][col][1] = '\0';
        fb_color[row][col] = color;
    }
}

// =========================================================================
// Connection-mask based line drawing
//
// Pass 1: Bresenham traces segments, ORing direction bits into conn_mask[][].
// Pass 2: Each cell's mask is resolved to the correct box-drawing glyph.
// This produces proper corners (┌┐└┘), T-junctions (├┤┬┴), and crosses (┼).
// =========================================================================

// Direction bits for cardinal connections
#define CONN_UP    0x01
#define CONN_RIGHT 0x02
#define CONN_DOWN  0x04
#define CONN_LEFT  0x08
// Diagonal connections
#define CONN_DIAG_FWD 0x10   // ╱  (screen: up-right / down-left)
#define CONN_DIAG_BWD 0x20   // ╲  (screen: down-right / up-left)

static uint8_t conn_mask[TOTAL_ROWS][SCREEN_W];

static void conn_clear(void) {
    memset(conn_mask, 0, sizeof(conn_mask));
}

static void conn_set(int row, int col, uint8_t bits) {
    if (row >= 0 && row < TOTAL_ROWS && col >= 0 && col < SCREEN_W)
        conn_mask[row][col] |= bits;
}

// Mark a connection between two adjacent cells
static void conn_link(int r1, int c1, int r2, int c2) {
    int dr = r2 - r1;  // +1 = down, -1 = up
    int dc = c2 - c1;  // +1 = right, -1 = left

    if (dr == 0 && dc > 0) {
        conn_set(r1, c1, CONN_RIGHT);
        conn_set(r2, c2, CONN_LEFT);
    } else if (dr == 0 && dc < 0) {
        conn_set(r1, c1, CONN_LEFT);
        conn_set(r2, c2, CONN_RIGHT);
    } else if (dc == 0 && dr > 0) {
        conn_set(r1, c1, CONN_DOWN);
        conn_set(r2, c2, CONN_UP);
    } else if (dc == 0 && dr < 0) {
        conn_set(r1, c1, CONN_UP);
        conn_set(r2, c2, CONN_DOWN);
    } else if (dr > 0 && dc > 0) {
        // down-right = ╲
        conn_set(r1, c1, CONN_DIAG_BWD);
        conn_set(r2, c2, CONN_DIAG_BWD);
    } else if (dr > 0 && dc < 0) {
        // down-left = ╱
        conn_set(r1, c1, CONN_DIAG_FWD);
        conn_set(r2, c2, CONN_DIAG_FWD);
    } else if (dr < 0 && dc > 0) {
        // up-right = ╱
        conn_set(r1, c1, CONN_DIAG_FWD);
        conn_set(r2, c2, CONN_DIAG_FWD);
    } else if (dr < 0 && dc < 0) {
        // up-left = ╲
        conn_set(r1, c1, CONN_DIAG_BWD);
        conn_set(r2, c2, CONN_DIAG_BWD);
    }
}

// Trace a Bresenham line, recording connections (not drawing yet)
static void conn_trace_line(int x1, int y1, int x2, int y2) {
    int c1 = game_to_col(x1), r1 = game_to_row(y1);
    int c2 = game_to_col(x2), r2 = game_to_row(y2);

    int dx = abs(c2 - c1), dy = abs(r2 - r1);
    int sx = c1 < c2 ? 1 : -1;
    int sy = r1 < r2 ? 1 : -1;
    int err = dx - dy;
    int prev_c = c1, prev_r = r1;
    int first = 1;

    for (;;) {
        if (!first)
            conn_link(prev_r, prev_c, r1, c1);
        else
            first = 0;

        if (c1 == c2 && r1 == r2) break;
        prev_c = c1;
        prev_r = r1;
        int e2 = 2 * err;
        int moved_h = 0, moved_v = 0;
        if (e2 > -dy) { err -= dy; c1 += sx; moved_h = 1; }
        if (e2 < dx)  { err += dx; r1 += sy; moved_v = 1; }
        (void)moved_h; (void)moved_v;
    }
}

// Resolve connection mask to the correct Unicode glyph
static const char *conn_resolve(uint8_t mask) {
    uint8_t card = mask & 0x0F;
    uint8_t diag = mask & 0x30;

    // If only diagonal, use diagonal chars
    if (card == 0) {
        if (diag == CONN_DIAG_FWD) return CH_DIAG_FWD;
        if (diag == CONN_DIAG_BWD) return CH_DIAG_BWD;
        if (diag == (CONN_DIAG_FWD | CONN_DIAG_BWD)) return CH_CROSS;
        return " ";
    }

    // Cardinal connections — 16 combos
    // If there are also diagonal bits, prefer the cardinal glyph
    switch (card) {
        // Single direction (endpoints / stubs)
        case CONN_UP:                             return CH_VERT;
        case CONN_DOWN:                           return CH_VERT;
        case CONN_LEFT:                           return CH_HORIZ;
        case CONN_RIGHT:                          return CH_HORIZ;

        // Two directions — straights
        case CONN_UP | CONN_DOWN:                 return CH_VERT;
        case CONN_LEFT | CONN_RIGHT:              return CH_HORIZ;

        // Two directions — corners
        case CONN_DOWN | CONN_RIGHT:              return CH_TL;     // ┌
        case CONN_DOWN | CONN_LEFT:               return CH_TR;     // ┐
        case CONN_UP | CONN_RIGHT:                return CH_BL;     // └
        case CONN_UP | CONN_LEFT:                 return CH_BR;     // ┘

        // Three directions — T-junctions
        case CONN_UP | CONN_DOWN | CONN_RIGHT:    return CH_T_RIGHT; // ├
        case CONN_UP | CONN_DOWN | CONN_LEFT:     return CH_T_LEFT;  // ┤
        case CONN_LEFT | CONN_RIGHT | CONN_DOWN:  return CH_T_DOWN;  // ┬
        case CONN_LEFT | CONN_RIGHT | CONN_UP:    return CH_T_UP;    // ┴

        // Four directions — cross
        case CONN_UP|CONN_DOWN|CONN_LEFT|CONN_RIGHT: return CH_CROSS; // ┼

        default: return CH_CROSS;
    }
}

// Draw all connected cells from the mask buffer
static void conn_draw(uint8_t color) {
    int r, c;
    for (r = 0; r < TOTAL_ROWS; r++)
        for (c = 0; c < SCREEN_W; c++)
            if (conn_mask[r][c])
                render_cell(r, c, conn_resolve(conn_mask[r][c]), color);
}

// Centered text on a screen row (ASCII only)
static void render_text(int row, const char *text, uint8_t color) {
    int len = (int)strlen(text);
    int col = (SCREEN_W - len) / 2;
    int i;
    for (i = 0; i < len; i++)
        render_char(row, col + i, text[i], color);
}

// Text at specific column (ASCII)
static void render_text_at(int row, int col, const char *text, uint8_t color) {
    int i;
    for (i = 0; text[i]; i++)
        render_char(row, col + i, text[i], color);
}

// =========================================================================
// Cave rendering — line-drawing characters
// =========================================================================

static void render_cave(void) {
    const int8_t *p = cur_cave_lines;
    uint8_t n;
    int8_t cx, cy;
    uint8_t cave_color = (dyn_exploding || (game_state == STATE_LEVEL_COMPLETE && (level_msg_timer & 4)))
                         ? (COL_WHITE | COL_BRIGHT) : COL_WHITE;

    // Pass 1: trace all segments into connection mask
    conn_clear();
    while ((n = (uint8_t)*p++) != 0) {
        cy = p[0];
        cx = p[1];
        p += 2;
        uint8_t i;
        for (i = 0; i < n; i++) {
            int8_t ny = cy + p[0];
            int8_t nx = cx + p[1];
            conn_trace_line(cx, cy, nx, ny);
            cy = ny;
            cx = nx;
            p += 2;
        }
    }

    // Pass 2: resolve masks to glyphs and draw
    conn_draw(cave_color);
}

static void render_walls(void) {
    uint8_t i;
    uint8_t wall_color = dyn_exploding ? (COL_YELLOW | COL_BRIGHT) : COL_YELLOW;
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        int wy = wall_y(i);
        int wx = wall_x(i);
        int wh = wall_h(i);
        int ww = wall_w(i);
        int c1 = game_to_col(wx);
        int c2 = game_to_col(wx + ww);
        int r1 = game_to_row(wy + wh);
        int r2 = game_to_row(wy - wh);
        int r, c;
        if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
        for (r = r1; r <= r2; r++)
            for (c = c1; c <= c2; c++) {
                // Border vs fill
                if (r == r1 || r == r2 || c == c1 || c == c2)
                    render_cell(r, c, CH_FULL, wall_color);
                else
                    render_cell(r, c, CH_DARK, wall_color);
            }
    }
}

static void render_lava(void) {
    if (!cur_has_lava) return;
    int r = game_to_row(cur_cave_floor);
    int c1 = game_to_col(cur_cave_left);
    int c2 = game_to_col(cur_cave_right);
    int c;
    uint8_t phase = (anim_tick >> 1) & 3;
    for (c = c1; c <= c2; c++) {
        const char *ch = ((c + phase) & 1) ? CH_LAVA1 : CH_LAVA2;
        render_cell(r, c, ch, COL_RED | COL_BRIGHT);
        ch = ((c + phase + 1) & 1) ? CH_DARK : CH_MED;
        render_cell(r + 1, c, ch, COL_RED);
    }
}

// =========================================================================
// Entity rendering
// =========================================================================

static void render_enemies_gfx(void) {
    uint8_t i;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        int c = game_to_col(enemies[i].x);
        int r = game_to_row(enemies[i].y);

        if (enemies[i].type == ENEMY_SPIDER) {
            //  Spider on thread:
            //
            //     │        thread
            //     │
            //    ╱●╲       head + legs    (row)
            //
            int anchor_row = game_to_row(enemies[i].home_y);
            int tr;
            for (tr = anchor_row; tr < r; tr++)
                render_cell(tr, c, CH_VERT, COL_MAGENTA);

            // Body — head + legs spread
            render_cell(r, c,     CH_BULLET,   COL_MAGENTA | COL_BRIGHT);  // ● body
            render_cell(r, c - 1, CH_DIAG_FWD, COL_MAGENTA);              // ╱ legs left
            render_cell(r, c + 1, CH_DIAG_BWD, COL_MAGENTA);              // ╲ legs right

        } else if (enemies[i].type == ENEMY_SNAKE) {
            const char *ch = (enemies[i].anim & 4) ? CH_SNAKE1 : CH_SNAKE2;
            render_cell(r, c, ch, COL_RED | COL_BRIGHT);
            // Wider body
            render_cell(r, c + (enemies[i].vx > 0 ? -1 : 1), CH_SNAKE1, COL_RED);

        } else {
            //  Bat — 2 animation frames, 3 wide:
            //
            //  Wings up:     ╱▼╲      (row-1)
            //                 ▾        (row)  tiny feet
            //
            //  Wings down:   ╲▲╱      (row-1)
            //                 ▴        (row)  tiny feet

            if (enemies[i].anim & 8) {
                // Wings up
                render_cell(r - 1, c - 1, CH_DIAG_FWD, COL_RED);                // ╱
                render_cell(r - 1, c,     "\u25BC",     COL_RED | COL_BRIGHT);   // ▼ body
                render_cell(r - 1, c + 1, CH_DIAG_BWD, COL_RED);                // ╲
                render_cell(r,     c,     "\u25BE",     COL_RED);                // ▾ feet
            } else {
                // Wings down
                render_cell(r - 1, c - 1, CH_DIAG_BWD, COL_RED);                // ╲
                render_cell(r - 1, c,     "\u25B2",     COL_RED | COL_BRIGHT);   // ▲ body
                render_cell(r - 1, c + 1, CH_DIAG_FWD, COL_RED);                // ╱
                render_cell(r,     c,     "\u25B4",     COL_RED);                // ▴ feet
            }
        }
    }
}

static void render_laser(void) {
    if (!laser_active) return;
    int r = game_to_row(laser_y);
    int c_start = game_to_col(laser_x);
    int c_end = game_to_col(laser_x + laser_dir * LASER_LENGTH);
    int c;
    uint8_t color = (anim_tick & 1) ? (COL_YELLOW | COL_BRIGHT) : COL_YELLOW;
    if (c_start > c_end) { int t = c_start; c_start = c_end; c_end = t; }
    for (c = c_start; c <= c_end; c++)
        render_cell(r, c, CH_LASER, color);
}

static void render_dynamite(void) {
    if (!dyn_active) return;

    if (!dyn_exploding) {
        int r = game_to_row(dyn_y);
        int c = game_to_col(dyn_x);
        render_cell(r, c, CH_DYN, COL_YELLOW | COL_BRIGHT);
        if (dyn_timer & 2)
            render_cell(r - 1, c, CH_FUSE, COL_YELLOW | COL_BRIGHT);
    } else {
        uint8_t age = EXPLOSION_TIME - dyn_expl_timer;
        int radius = age * 3;
        if (radius > EXPLOSION_RADIUS) radius = EXPLOSION_RADIUS;

        int cr = game_to_row(dyn_y);
        int cc = game_to_col(dyn_x);
        int screen_r = radius * SCREEN_H / 256 + 1;
        int screen_c = radius * SCREEN_W / 256 + 1;
        uint8_t color = age < 5 ? (COL_WHITE | COL_BRIGHT) :
                        age < 10 ? (COL_YELLOW | COL_BRIGHT) : COL_YELLOW;
        int r, c;

        for (r = cr - screen_r; r <= cr + screen_r; r++) {
            for (c = cc - screen_c; c <= cc + screen_c; c++) {
                int dr = abs(r - cr), dc = abs(c - cc);
                if (age < 4) {
                    // Full starburst
                    if (dr <= 1 && dc <= 1)
                        render_cell(r, c, CH_STAR, COL_WHITE | COL_BRIGHT);
                    else if (dr == 0 || dc == 0)
                        render_cell(r, c, CH_BULLET, color);
                } else if (dr == dc) {
                    // X pattern
                    const char *ch = age < 10 ? CH_SPARK : CH_LIGHT;
                    render_cell(r, c, ch, color);
                } else if (dr == 0 || dc == 0) {
                    // + pattern
                    const char *ch = age < 10 ? CH_SPARK : CH_LIGHT;
                    render_cell(r, c, ch, color);
                }
            }
        }
        // Center
        if (age < 8)
            render_cell(cr, cc, CH_STAR, COL_WHITE | COL_BRIGHT);
    }
}

static void render_player(void) {
    if (game_state == STATE_DYING && !(death_timer & 2)) return;

    int c = game_to_col(player_x);
    int r = game_to_row(player_y);
    uint8_t body_col = player_thrusting ? (COL_GREEN | COL_BRIGHT) : COL_GREEN;
    int f = player_facing;   // +1 right, -1 left
    int flying = !player_on_ground || player_thrusting;

    //  Layout (facing right):
    //
    //  row-2:  ─ or ⨯    propeller (when flying, animated)
    //  row-1:    ○        head
    //  row:    ─ ■ ▶      arm, body, direction
    //  row+1:   ╱ ╲       feet (animated when walking)
    //           or ╵╵     (dangling when flying)

    // --- Row -2: propeller (flying only) ---
    if (flying) {
        if (anim_tick & 2) {
            // Blades horizontal
            render_cell(r - 2, c - 1, CH_HORIZ, COL_CYAN | COL_BRIGHT);
            render_cell(r - 2, c,     CH_HORIZ, COL_CYAN | COL_BRIGHT);
            render_cell(r - 2, c + 1, CH_HORIZ, COL_CYAN | COL_BRIGHT);
        } else {
            // Blades angled
            render_cell(r - 2, c - 1, CH_DIAG_FWD, COL_CYAN);
            render_cell(r - 2, c,     CH_BULLET,   COL_CYAN | COL_BRIGHT);
            render_cell(r - 2, c + 1, CH_DIAG_BWD, COL_CYAN);
        }
        // Shaft
        render_cell(r - 2, c, (anim_tick & 2) ? CH_BELT : CH_BULLET, COL_CYAN | COL_BRIGHT);
    }

    // --- Row -1: head + helmet ---
    render_cell(r - 1, c, CH_HEAD, COL_WHITE | COL_BRIGHT);
    // Propeller mount (vertical rod from head to blades)
    if (flying)
        render_cell(r - 1, c, CH_HEAD, COL_WHITE | COL_BRIGHT);

    // --- Row 0: body + arm + facing ---
    render_cell(r, c, CH_BODY, body_col);

    if (f > 0) {
        // Facing right
        render_cell(r, c + 1, CH_ARROW_R, body_col);
        render_cell(r, c - 1, CH_ARM_R,   body_col);
    } else {
        // Facing left
        render_cell(r, c - 1, CH_ARROW_L, body_col);
        render_cell(r, c + 1, CH_ARM_L,   body_col);
    }

    // --- Row +1: legs/feet ---
    if (flying) {
        // Dangling legs + thrust flame
        render_cell(r + 1, c, CH_LEG_HANG, COL_GREEN);
        if (player_thrusting) {
            // Jet flame below legs
            const char *flame = (anim_tick & 2) ? CH_FLAME1 : CH_FLAME2;
            uint8_t fcol = (anim_tick & 2) ? (COL_YELLOW | COL_BRIGHT) : (COL_RED | COL_BRIGHT);
            render_cell(r + 2, c, flame, fcol);
        }
    } else if (player_vx != 0) {
        // Walking animation — alternating stride
        if (anim_tick & 4) {
            render_cell(r + 1, c - 1, CH_STRIDE_L, COL_GREEN);
            render_char(r + 1, c + 1, ' ', COL_BLACK);
        } else {
            render_char(r + 1, c - 1, ' ', COL_BLACK);
            render_cell(r + 1, c + 1, CH_STRIDE_R, COL_GREEN);
        }
    } else {
        // Standing still — feet together
        render_cell(r + 1, c - 1, CH_FOOT_L, COL_GREEN);
        render_cell(r + 1, c + 1, CH_FOOT_R, COL_GREEN);
    }
}

static void render_miner(void) {
    if (!cur_has_miner) return;
    int c = game_to_col(cur_miner_x);
    int r = game_to_row(cur_miner_y);
    uint8_t blink = (anim_tick & 8);
    uint8_t head_col = blink ? (COL_CYAN | COL_BRIGHT) : COL_CYAN;
    uint8_t body_col = COL_CYAN;

    //  Sitting miner (side view, legs to the right):
    //
    //  row-1:   ☻•     head + helmet lamp
    //  row:     ■╶▄    body, arm on knee, bent legs

    // Head — with helmet lamp blinking
    render_cell(r - 1, c, CH_MINER, head_col);
    if (blink)
        render_cell(r - 1, c + 1, CH_SPARK, COL_YELLOW | COL_BRIGHT);

    // Body + legs bent to the side
    render_cell(r, c,     CH_BODY,   body_col);          // ■ torso
    render_cell(r, c + 1, "\u2576",  body_col);          // ╶ arm/knee
    render_cell(r, c + 2, "\u2584",  body_col);          // ▄ bent legs/feet
}

// =========================================================================
// HUD with Unicode
// =========================================================================

static void render_hud(void) {
    char buf[SCREEN_W + 1];
    int i;

    // Row 0: DYN, SCORE, LIVES, LEVEL/ROOM

    // Dynamite indicator
    render_text_at(0, 1, "DYN:", COL_WHITE);
    for (i = 0; i < (int)START_DYNAMITE; i++) {
        if (i < (int)player_dynamite)
            render_cell(0, 5 + i * 2, CH_DYN, COL_YELLOW | COL_BRIGHT);
        else
            render_cell(0, 5 + i * 2, CH_LIGHT, COL_WHITE);
    }

    // Score
    snprintf(buf, sizeof(buf), "SCORE:%d", score);
    render_text_at(0, 20, buf, COL_WHITE | COL_BRIGHT);

    // Lives as hearts
    render_text_at(0, 45, "LIVES:", COL_WHITE);
    for (i = 0; i < (int)START_LIVES; i++) {
        if (i < (int)player_lives)
            render_cell(0, 51 + i * 2, CH_HEART, COL_RED | COL_BRIGHT);
        else
            render_cell(0, 51 + i * 2, CH_LIGHT, COL_WHITE);
    }

    // Level-room
    snprintf(buf, sizeof(buf), "L%d-R%d", current_level + 1, current_room);
    render_text_at(0, 65, buf, COL_CYAN);

    // Row 1: Fuel bar with block characters
    render_text_at(1, 1, "FUEL:", COL_WHITE);
    render_cell(1, 6, "\u2595", COL_WHITE);  // ▕
    int bar_w = 40;
    int filled = (int)player_fuel * bar_w / START_FUEL;
    for (i = 0; i < bar_w; i++) {
        if (i < filled) {
            uint8_t c = (filled > bar_w / 4) ? (COL_GREEN | COL_BRIGHT) :
                        (filled > bar_w / 8) ? (COL_YELLOW | COL_BRIGHT) :
                        (COL_RED | COL_BRIGHT);
            render_cell(1, 7 + i, CH_FULL, c);
        } else {
            render_cell(1, 7 + i, CH_LIGHT, COL_WHITE);
        }
    }
    render_cell(1, 7 + bar_w, "\u258F", COL_WHITE);  // ▏

    // Controls hint
    render_text_at(1, 55, "SPC:laser D:bomb", COL_BLUE);
}

// =========================================================================
// Full-screen displays
// =========================================================================

void render_title_screen(void) {
    render_clear();

    // Animated cave walls with line-drawing chars
    uint8_t base = (anim_tick >> 3) % 12;
    static const int8_t cave_wx[] = {30, 33, 28, 36, 31, 27, 34, 30, 32, 26, 35, 31};
    int center = SCREEN_W / 2;
    int r;
    for (r = 5; r < TOTAL_ROWS - 3; r++) {
        int idx = (base + (r - 5) / 3) % 12;
        int w = cave_wx[idx];
        int prev_idx = (base + (r - 6) / 3) % 12;
        int prev_w = (r > 5) ? cave_wx[prev_idx] : w;

        // Left wall — choose character based on wall shape
        const char *lch, *rch;
        if (w == prev_w) {
            lch = CH_VERT; rch = CH_VERT;
        } else if (w > prev_w) {
            lch = CH_DIAG_FWD; rch = CH_DIAG_BWD;
        } else {
            lch = CH_DIAG_BWD; rch = CH_DIAG_FWD;
        }
        render_cell(r, center - w, lch, COL_WHITE);
        render_cell(r, center + w, rch, COL_WHITE);
        // Subtle wall texture
        if ((r + anim_tick) & 3) {
            render_cell(r, center - w + 1, CH_LIGHT, COL_WHITE);
            render_cell(r, center + w - 1, CH_LIGHT, COL_WHITE);
        }
    }

    // Title — pulsing
    uint8_t phase = (anim_tick >> 1) & 15;
    uint8_t tri = phase < 8 ? phase : 15 - phase;
    uint8_t color = tri > 4 ? (COL_YELLOW | COL_BRIGHT) : COL_YELLOW;

    // Top border
    render_text(10, "+-----------------+", COL_YELLOW);
    render_text(11, "|                 |", COL_YELLOW);
    render_text(12, "| H . E . R . O. |", color);
    render_text(13, "|                 |", COL_YELLOW);
    render_text(14, "+-----------------+", COL_YELLOW);
    render_text(16, "Console Edition", COL_CYAN | COL_BRIGHT);

    render_text(22, "Navigate caves, rescue miners!", COL_WHITE);
    render_text(24, "Arrows:move  Space:laser  D:dynamite", COL_WHITE);
    render_text(28, "[ Press ENTER or SPACE to start ]", COL_GREEN | COL_BRIGHT);
    render_text(32, "Q: quit", COL_WHITE);
}

void render_level_intro_screen(void) {
    char buf[32];
    render_clear();

    // Decorative border
    render_text(15, "========================", COL_YELLOW);
    snprintf(buf, sizeof(buf), "LEVEL %d", current_level + 1);
    render_text(17, buf, COL_YELLOW | COL_BRIGHT);
    render_text(19, "========================", COL_YELLOW);
    render_text(22, "Get ready!", COL_WHITE);
}

void render_game_over_screen(void) {
    char buf[32];
    render_clear();
    render_text(13, "========================", COL_RED);
    render_text(15, "G A M E   O V E R", COL_RED | COL_BRIGHT);
    render_text(17, "========================", COL_RED);
    snprintf(buf, sizeof(buf), "Final Score: %d", score);
    render_text(20, buf, COL_WHITE | COL_BRIGHT);
    render_text(25, "[ Press ENTER to continue ]", COL_WHITE);
}

void render_rescued_screen(void) {
    char buf[32];
    render_clear();
    render_text(13, "============================", COL_CYAN);
    render_text(15, "M I N E R   R E S C U E D !", COL_CYAN | COL_BRIGHT);
    render_text(17, "============================", COL_CYAN);
    snprintf(buf, sizeof(buf), "Score: %d", score);
    render_text(20, buf, COL_WHITE | COL_BRIGHT);
    render_text(25, "[ Press ENTER to continue ]", COL_WHITE);
}

void render_failed_screen(void) {
    char buf[32];
    render_clear();
    render_text(13, "========================", COL_RED);
    render_text(15, "L E V E L   F A I L E D", COL_RED | COL_BRIGHT);
    render_text(17, "========================", COL_RED);
    snprintf(buf, sizeof(buf), "Score: %d", score);
    render_text(20, buf, COL_WHITE);
    render_text(25, "[ Press ENTER to continue ]", COL_WHITE);
}

// =========================================================================
// Gameplay frame
// =========================================================================

void render_game(void) {
    render_clear();
    render_cave();
    render_walls();
    render_lava();
    render_enemies_gfx();
    render_dynamite();
    render_laser();
    render_player();
    render_miner();
    render_hud();
}

// =========================================================================
// Flush to terminal with ANSI colors
// =========================================================================

static const char *ansi_fg[] = {
    "\033[30m", "\033[31m", "\033[32m", "\033[33m",
    "\033[34m", "\033[35m", "\033[36m", "\033[37m",
    "\033[90m", "\033[91m", "\033[92m", "\033[93m",
    "\033[94m", "\033[95m", "\033[96m", "\033[97m",
};

void render_flush(void) {
    int r, c;
    uint8_t prev_color = 255;

    out_pos = 0;
    out_str("\033[H");

    for (r = 0; r < TOTAL_ROWS; r++) {
        for (c = 0; c < SCREEN_W; c++) {
            uint8_t color = fb_color[r][c];
            if (color != prev_color) {
                uint8_t idx = (color & 0x07) | ((color & COL_BRIGHT) ? 8 : 0);
                out_str(ansi_fg[idx]);
                prev_color = color;
            }
            out_str(fb_char[r][c]);
        }
        if (r < TOTAL_ROWS - 1)
            out_str("\r\n");
    }

    out_str("\033[0m");
    write(STDOUT_FILENO, out_buf, out_pos);
}
