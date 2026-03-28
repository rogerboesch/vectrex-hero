//
// render.c — Tilemap generation, sprite management, HUD
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// Cave grid: rasterize polylines -> flood fill -> auto-tile
// =========================================================================

#define CELL_SOLID  2   // outside cave (unreached by flood fill)
#define CELL_BORDER 1   // polyline boundary
#define CELL_EMPTY  0   // inside cave

static uint8_t cave_grid[PLAY_ROWS][PLAY_COLS];
static uint8_t tilemap[18][20];
static uint8_t attrmap[18][20];

// Bresenham line: mark cells in cave_grid as border
static void trace_line(int8_t gx1, int8_t gy1, int8_t gx2, int8_t gy2) {
    int16_t c1 = TILE_COL(gx1), r1 = TILE_ROW(gy1);
    int16_t c2 = TILE_COL(gx2), r2 = TILE_ROW(gy2);
    int16_t dx = c2 - c1; if (dx < 0) dx = -dx;
    int16_t dy = r2 - r1; if (dy < 0) dy = -dy;
    int16_t sx = c1 < c2 ? 1 : -1;
    int16_t sy = r1 < r2 ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    for (;;) {
        if (r1 >= 0 && r1 < PLAY_ROWS && c1 >= 0 && c1 < PLAY_COLS)
            cave_grid[r1][c1] = CELL_BORDER;
        if (c1 == c2 && r1 == r2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; c1 += sx; }
        if (e2 < dx)  { err += dx; r1 += sy; }
    }
}

// Trace all polylines from cave_lines data
static void trace_cave_polylines(void) {
    const int8_t *p = cur_cave_lines;
    uint8_t n;
    int8_t cx, cy;

    while ((n = (uint8_t)*p++) != 0) {
        cy = p[0]; cx = p[1]; p += 2;
        uint8_t i;
        for (i = 0; i < n; i++) {
            int8_t ny = cy + p[0];
            int8_t nx = cx + p[1];
            trace_line(cx, cy, nx, ny);
            cy = ny; cx = nx;
            p += 2;
        }
    }
}

// Flood fill from player start to mark interior as empty.
// Iterative multi-pass (no extra memory needed).
static void flood_fill(uint8_t sr, uint8_t sc) {
    uint8_t changed, r, c;

    if (sr < PLAY_ROWS && sc < PLAY_COLS)
        cave_grid[sr][sc] = CELL_EMPTY;

    changed = 1;
    while (changed) {
        changed = 0;
        for (r = 0; r < PLAY_ROWS; r++) {
            for (c = 0; c < PLAY_COLS; c++) {
                if (cave_grid[r][c] != CELL_SOLID) continue;
                if ((r > 0              && cave_grid[r-1][c] == CELL_EMPTY) ||
                    (r < PLAY_ROWS - 1  && cave_grid[r+1][c] == CELL_EMPTY) ||
                    (c > 0              && cave_grid[r][c-1] == CELL_EMPTY) ||
                    (c < PLAY_COLS - 1  && cave_grid[r][c+1] == CELL_EMPTY)) {
                    cave_grid[r][c] = CELL_EMPTY;
                    changed = 1;
                }
            }
        }
    }
}

// Convert cave_grid to tilemap with auto-tiled walls
static void build_tilemap(void) {
    uint8_t r, c, mask;

    for (r = 0; r < PLAY_ROWS; r++) {
        for (c = 0; c < PLAY_COLS; c++) {
            if (cave_grid[r][c] == CELL_EMPTY) {
                tilemap[r + HUD_ROWS][c] = TILE_EMPTY;
                attrmap[r + HUD_ROWS][c] = PAL_BG_EMPTY;
            } else {
                // Auto-tile: check which neighbors are also wall
                mask = 0;
                if (r > 0              && cave_grid[r-1][c] != CELL_EMPTY) mask |= 0x01; // up
                if (c < PLAY_COLS - 1  && cave_grid[r][c+1] != CELL_EMPTY) mask |= 0x02; // right
                if (r < PLAY_ROWS - 1  && cave_grid[r+1][c] != CELL_EMPTY) mask |= 0x04; // down
                if (c > 0              && cave_grid[r][c-1] != CELL_EMPTY) mask |= 0x08; // left
                tilemap[r + HUD_ROWS][c] = TILE_WALL + mask;
                attrmap[r + HUD_ROWS][c] = PAL_BG_CAVE;
            }
        }
    }

    // Overlay destroyable walls
    {
        uint8_t i;
        for (i = 0; i < cur_wall_count; i++) {
            int8_t wy = wall_y(i);
            int8_t wx = wall_x(i);
            int8_t wh = wall_h(i);
            int8_t ww = wall_w(i);
            uint8_t r1 = TILE_ROW(wy + wh);
            uint8_t r2 = TILE_ROW(wy - wh);
            uint8_t c1 = TILE_COL(wx);
            uint8_t c2 = TILE_COL(wx + ww);
            uint8_t tr, tc;
            if (r1 > r2) { uint8_t t = r1; r1 = r2; r2 = t; }
            for (tr = r1; tr <= r2 && tr < PLAY_ROWS; tr++) {
                for (tc = c1; tc <= c2 && tc < PLAY_COLS; tc++) {
                    if (!(walls_destroyed & (1 << i))) {
                        tilemap[tr + HUD_ROWS][tc] = (tr == r1 || tr == r2 || tc == c1 || tc == c2)
                                                      ? TILE_DWALL_EDGE : TILE_DWALL;
                        attrmap[tr + HUD_ROWS][tc] = PAL_BG_DWALL;
                    }
                }
            }
        }
    }

    // Lava at bottom
    if (cur_has_lava) {
        uint8_t c1 = TILE_COL(cur_cave_left);
        uint8_t c2 = TILE_COL(cur_cave_right);
        uint8_t lr = TILE_ROW(cur_cave_floor);
        uint8_t tc;
        if (lr < PLAY_ROWS) {
            for (tc = c1; tc <= c2 && tc < PLAY_COLS; tc++) {
                tilemap[lr + HUD_ROWS][tc] = TILE_LAVA0;
                attrmap[lr + HUD_ROWS][tc] = PAL_BG_LAVA;
            }
        }
    }
}

// =========================================================================
// HUD tilemap (rows 0-1)
// =========================================================================

static void build_hud_tilemap(void) {
    uint8_t c;

    // Clear HUD rows
    for (c = 0; c < 20; c++) {
        tilemap[0][c] = TILE_EMPTY;
        tilemap[1][c] = TILE_EMPTY;
        attrmap[0][c] = PAL_BG_HUD;
        attrmap[1][c] = PAL_BG_HUD;
    }

    // Row 0: L<n> | hearts | dyn icons | SC:<score>
    // Level number
    tilemap[0][0] = TILE_LETTER_L;
    tilemap[0][1] = TILE_DIGIT_0 + ((current_level + 1) / 10);
    tilemap[0][2] = TILE_DIGIT_0 + ((current_level + 1) % 10);

    // Hearts (lives)
    tilemap[0][4] = (player_lives >= 1) ? TILE_HEART : TILE_HEART_OFF;
    tilemap[0][5] = (player_lives >= 2) ? TILE_HEART : TILE_HEART_OFF;
    tilemap[0][6] = (player_lives >= 3) ? TILE_HEART : TILE_HEART_OFF;
    attrmap[0][4] = PAL_BG_HUD2;
    attrmap[0][5] = PAL_BG_HUD2;
    attrmap[0][6] = PAL_BG_HUD2;

    // Dynamite icons
    tilemap[0][8]  = (player_dynamite >= 1) ? TILE_DYN_ICON : TILE_DYN_OFF;
    tilemap[0][9]  = (player_dynamite >= 2) ? TILE_DYN_ICON : TILE_DYN_OFF;
    tilemap[0][10] = (player_dynamite >= 3) ? TILE_DYN_ICON : TILE_DYN_OFF;
    attrmap[0][8]  = PAL_BG_DWALL;
    attrmap[0][9]  = PAL_BG_DWALL;
    attrmap[0][10] = PAL_BG_DWALL;

    // Score
    {
        int16_t s = score;
        uint8_t d;
        tilemap[0][13] = TILE_LETTER_S;
        tilemap[0][14] = TILE_LETTER_C;
        tilemap[0][15] = TILE_COLON;
        // 4-digit score right-aligned at cols 16-19
        for (d = 0; d < 4; d++) {
            tilemap[0][19 - d] = TILE_DIGIT_0 + (s % 10);
            s /= 10;
        }
    }

    // Row 1: Fuel bar (20 tiles wide)
    {
        uint8_t filled = (uint8_t)((uint16_t)player_fuel * 20 / START_FUEL);
        for (c = 0; c < 20; c++) {
            tilemap[1][c] = (c < filled) ? TILE_HUD_FILL : TILE_HUD_EMPTY;
            attrmap[1][c] = PAL_BG_HUD;
        }
    }
}

// =========================================================================
// Public: initialize room tilemap
// =========================================================================

void render_init_room(void) {
    uint8_t sr, sc;

    // 1. Fill grid as solid
    memset(cave_grid, CELL_SOLID, sizeof(cave_grid));

    // 2. Trace cave polylines onto grid
    trace_cave_polylines();

    // 3. Flood fill from player start
    sr = TILE_ROW(room_starts[current_room * 2 + 1]);
    sc = TILE_COL(room_starts[current_room * 2]);
    flood_fill(sr, sc);

    // 4. Build tilemap + HUD
    build_tilemap();
    build_hud_tilemap();

    // 5. Upload to VRAM (LCD should be off or in vblank)
    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 18, &tilemap[0][0]);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, 20, 18, &attrmap[0][0]);
    VBK_REG = 0;
}

// =========================================================================
// Public: update HUD tiles (call each frame or when changed)
// =========================================================================

void render_update_hud(void) {
    build_hud_tilemap();
    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 2, &tilemap[0][0]);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, 20, 2, &attrmap[0][0]);
    VBK_REG = 0;
}

// =========================================================================
// Public: update when a wall is destroyed
// =========================================================================

void render_destroy_wall(uint8_t idx) {
    int8_t wy = wall_y(idx);
    int8_t wx = wall_x(idx);
    int8_t wh = wall_h(idx);
    int8_t ww = wall_w(idx);
    uint8_t r1 = TILE_ROW(wy + wh);
    uint8_t r2 = TILE_ROW(wy - wh);
    uint8_t c1 = TILE_COL(wx);
    uint8_t c2 = TILE_COL(wx + ww);
    uint8_t tr, tc;
    uint8_t row[20];

    if (r1 > r2) { uint8_t t = r1; r1 = r2; r2 = t; }
    for (tr = r1; tr <= r2 && tr < PLAY_ROWS; tr++) {
        for (tc = c1; tc <= c2 && tc < PLAY_COLS; tc++) {
            tilemap[tr + HUD_ROWS][tc] = TILE_EMPTY;
            attrmap[tr + HUD_ROWS][tc] = PAL_BG_EMPTY;
        }
        // Update this row in VRAM
        memcpy(row, &tilemap[tr + HUD_ROWS][0], 20);
        VBK_REG = 0;
        set_bkg_tiles(0, tr + HUD_ROWS, 20, 1, row);
        memcpy(row, &attrmap[tr + HUD_ROWS][0], 20);
        VBK_REG = 1;
        set_bkg_tiles(0, tr + HUD_ROWS, 20, 1, row);
        VBK_REG = 0;
    }
}

// =========================================================================
// Lava animation (swap tile data)
// =========================================================================

static void render_lava_anim(void) {
    if (!cur_has_lava) return;
    uint8_t lr = TILE_ROW(cur_cave_floor);
    uint8_t c1 = TILE_COL(cur_cave_left);
    uint8_t c2 = TILE_COL(cur_cave_right);
    uint8_t tc, frame;
    frame = (anim_tick >> 3) & 1;
    if (lr < PLAY_ROWS) {
        for (tc = c1; tc <= c2 && tc < PLAY_COLS; tc++) {
            tilemap[lr + HUD_ROWS][tc] = ((tc + frame) & 1) ? TILE_LAVA0 : TILE_LAVA1;
        }
        VBK_REG = 0;
        set_bkg_tiles(0, lr + HUD_ROWS, 20, 1, &tilemap[lr + HUD_ROWS][0]);
        VBK_REG = 0;
    }
}

// =========================================================================
// Public: update all sprites each frame
// =========================================================================

void render_update_sprites(void) {
    uint8_t i, spr_tile, spr_prop;
    uint8_t px, py;

    // Lava animation
    render_lava_anim();

    // --- Player sprite ---
    if (game_state == STATE_DYING && !(death_timer & 2)) {
        move_sprite(OAM_PLAYER, 0, 0);  // hide
        move_sprite(OAM_PROP, 0, 0);
    } else {
        px = SPR_X(player_x);
        py = SPR_Y(player_y);

        // Choose sprite tile based on state
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

        // Propeller (when flying)
        if (!player_on_ground || player_thrusting) {
            spr_prop = (anim_tick & 4) ? SPR_PROP0 : SPR_PROP1;
            set_sprite_tile(OAM_PROP, spr_prop);
            set_sprite_prop(OAM_PROP, PAL_SP_LASER);  // yellow propeller
            move_sprite(OAM_PROP, px, py - 16);
        } else {
            move_sprite(OAM_PROP, 0, 0);
        }
    }

    // --- Enemy sprites ---
    for (i = 0; i < 3; i++) {
        uint8_t oam = OAM_ENEMY0 + i;
        if (i < enemy_count && enemies[i].alive) {
            uint8_t ex = SPR_X(enemies[i].x);
            uint8_t ey = SPR_Y(enemies[i].y);

            if (enemies[i].type == ENEMY_SPIDER) {
                set_sprite_tile(oam, SPR_SPIDER);
                set_sprite_prop(oam, PAL_SP_SPIDER);
            } else if (enemies[i].type == ENEMY_SNAKE) {
                set_sprite_tile(oam, SPR_SNAKE_R);
                set_sprite_prop(oam, PAL_SP_SNAKE |
                                ((enemies[i].vx < 0) ? S_FLIPX : 0));
            } else {
                set_sprite_tile(oam, (enemies[i].anim & 8) ? SPR_BAT0 : SPR_BAT1);
                set_sprite_prop(oam, PAL_SP_BAT);
            }
            move_sprite(oam, ex, ey);
        } else {
            move_sprite(oam, 0, 0);  // hide
        }
    }

    // --- Miner sprite ---
    if (cur_has_miner) {
        set_sprite_tile(OAM_MINER, SPR_MINER);
        set_sprite_prop(OAM_MINER, PAL_SP_MINER);
        move_sprite(OAM_MINER, SPR_X(cur_miner_x), SPR_Y(cur_miner_y));
    } else {
        move_sprite(OAM_MINER, 0, 0);
    }

    // --- Dynamite / explosion sprite ---
    if (dyn_active) {
        if (dyn_exploding) {
            set_sprite_tile(OAM_DYNAMITE, SPR_EXPLODE);
            set_sprite_prop(OAM_DYNAMITE, PAL_SP_LASER);
        } else {
            set_sprite_tile(OAM_DYNAMITE, SPR_DYNAMITE);
            set_sprite_prop(OAM_DYNAMITE, PAL_SP_LASER);
        }
        move_sprite(OAM_DYNAMITE, SPR_X(dyn_x), SPR_Y(dyn_y));
    } else {
        move_sprite(OAM_DYNAMITE, 0, 0);
    }

    // --- Laser sprites (up to 3 segments) ---
    if (laser_active) {
        int8_t lx;
        uint8_t step = LASER_LENGTH / 3;
        for (i = 0; i < 3; i++) {
            lx = laser_x + laser_dir * step * (i + 1);
            set_sprite_tile(OAM_LASER0 + i, SPR_LASER);
            set_sprite_prop(OAM_LASER0 + i, PAL_SP_LASER);
            move_sprite(OAM_LASER0 + i, SPR_X(lx), SPR_Y(laser_y));
        }
    } else {
        for (i = 0; i < 3; i++)
            move_sprite(OAM_LASER0 + i, 0, 0);
    }
}

// =========================================================================
// Public: hide all sprites (for menu screens)
// =========================================================================

void render_hide_sprites(void) {
    uint8_t i;
    for (i = 0; i < OAM_COUNT; i++)
        move_sprite(i, 0, 0);
}

// =========================================================================
// Public: title screen
// =========================================================================

void render_title(void) {
    uint8_t r, c;

    render_hide_sprites();

    // Clear tilemap to empty
    for (r = 0; r < 18; r++)
        for (c = 0; c < 20; c++) {
            tilemap[r][c] = TILE_EMPTY;
            attrmap[r][c] = PAL_BG_EMPTY;
        }

    // "HERO" at center using letter tiles
    // H = approximate with wall tiles, or use available letters
    // Just spell with what we have: "HERO" => use walls as decoration
    // Row 7-9: decorative cave walls
    for (c = 0; c < 20; c++) {
        tilemap[4][c] = TILE_WALL + 0x0F;  // solid interior
        tilemap[13][c] = TILE_WALL + 0x0F;
        attrmap[4][c] = PAL_BG_CAVE;
        attrmap[13][c] = PAL_BG_CAVE;
    }

    // Title text: "SC:HERO" using available tiles
    tilemap[8][7]  = TILE_LETTER_S;  // H approximated
    tilemap[8][8]  = TILE_LETTER_E;
    tilemap[8][9]  = TILE_LETTER_R;
    tilemap[8][10] = TILE_LETTER_L;  // O approximated
    attrmap[8][7]  = PAL_BG_DWALL;  // yellow text
    attrmap[8][8]  = PAL_BG_DWALL;
    attrmap[8][9]  = PAL_BG_DWALL;
    attrmap[8][10] = PAL_BG_DWALL;

    // "LV" below
    tilemap[10][8]  = TILE_LETTER_L;
    tilemap[10][9]  = TILE_LETTER_V;
    attrmap[10][8]  = PAL_BG_HUD;
    attrmap[10][9]  = PAL_BG_HUD;

    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 18, &tilemap[0][0]);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, 20, 18, &attrmap[0][0]);
    VBK_REG = 0;
}

// =========================================================================
// Public: message screen (level intro, game over, etc.)
// =========================================================================

void render_msg(const char *line1, const char *line2) {
    uint8_t r, c, i;
    char ch;

    (void)line2;
    render_hide_sprites();

    for (r = 0; r < 18; r++)
        for (c = 0; c < 20; c++) {
            tilemap[r][c] = TILE_EMPTY;
            attrmap[r][c] = PAL_BG_EMPTY;
        }

    // Decorative borders
    for (c = 0; c < 20; c++) {
        tilemap[6][c] = TILE_WALL + 0x0A;  // horizontal bar
        tilemap[11][c] = TILE_WALL + 0x0A;
        attrmap[6][c] = PAL_BG_CAVE;
        attrmap[11][c] = PAL_BG_CAVE;
    }

    // Render line1 centered on row 8
    i = 0;
    while (line1[i]) i++;
    c = (20 - i) / 2;
    for (i = 0; line1[i] && c < 20; i++, c++) {
        ch = line1[i];
        if (ch >= '0' && ch <= '9')
            tilemap[8][c] = TILE_DIGIT_0 + (ch - '0');
        else if (ch == 'L') tilemap[8][c] = TILE_LETTER_L;
        else if (ch == 'V') tilemap[8][c] = TILE_LETTER_V;
        else if (ch == 'S') tilemap[8][c] = TILE_LETTER_S;
        else if (ch == 'C') tilemap[8][c] = TILE_LETTER_C;
        else if (ch == 'R') tilemap[8][c] = TILE_LETTER_R;
        else if (ch == 'E') tilemap[8][c] = TILE_LETTER_E;
        else if (ch == '-') tilemap[8][c] = TILE_LETTER_DASH;
        else if (ch == ':') tilemap[8][c] = TILE_COLON;
        else tilemap[8][c] = TILE_EMPTY;
        attrmap[8][c] = PAL_BG_DWALL;
    }

    // Render line2 on row 9 if provided
    if (line2 != 0) {
        i = 0;
        while (line2[i]) i++;
        c = (20 - i) / 2;
        for (i = 0; line2[i] && c < 20; i++, c++) {
            ch = line2[i];
            if (ch >= '0' && ch <= '9')
                tilemap[9][c] = TILE_DIGIT_0 + (ch - '0');
            else if (ch == ':') tilemap[9][c] = TILE_COLON;
            else if (ch == 'S') tilemap[9][c] = TILE_LETTER_S;
            else if (ch == 'C') tilemap[9][c] = TILE_LETTER_C;
            else tilemap[9][c] = TILE_EMPTY;
            attrmap[9][c] = PAL_BG_HUD;
        }
    }

    VBK_REG = 0;
    set_bkg_tiles(0, 0, 20, 18, &tilemap[0][0]);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, 20, 18, &attrmap[0][0]);
    VBK_REG = 0;
}
