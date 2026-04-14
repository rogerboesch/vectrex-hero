//
// levels.c — Level loading, RLE decode, tile access (tilemap version)
//

#include "game.h"
#include "tiles.h"

u8 level_w, level_h;
u8 decode_cache[DECODE_ROWS][DECODE_MAX_W];

LevelEntity level_entities[MAX_LEVEL_ENTITIES];
u8 level_entity_count;

static u8 decode_valid[DECODE_ROWS]; // which rows are decoded

// =========================================================================
// RLE row decoder: decode a single row from ROM into decode_cache
// =========================================================================

void decode_row(u8 row) {
    if (row >= level_h) return;
    if (decode_valid[row & (DECODE_ROWS - 1)] == row + 1) return; // already decoded

    const LevelInfo *li = &levels[current_level];

    u16 offset = li->row_offsets[row];
    const u8 *rle = li->rle + offset;
    u8 *dst = decode_cache[row & (DECODE_ROWS - 1)];
    u8 col = 0;

    while (col < level_w) {
        u8 run = *rle++;
        u8 tile = *rle++;
        while (run-- && col < level_w)
            dst[col++] = tile;
    }
    decode_valid[row & (DECODE_ROWS - 1)] = row + 1;
}

// =========================================================================
// Tile access (reads from decode cache)
// =========================================================================

u8 tile_at(u8 tx, u8 ty) {
    if (tx >= level_w || ty >= level_h) return 1; // solid out of bounds
    decode_row(ty);
    return decode_cache[ty & (DECODE_ROWS - 1)][tx];
}

u8 tile_solid(u8 tx, u8 ty) {
    u8 t = tile_at(tx, ty);
    if (t == 0) return 0;
    if (t >= TILE_DECOR_FIRST && t <= TILE_DECOR_LAST) return 0;
    return 1;
}

// =========================================================================
// Decode a range of rows (for initial screen fill)
// =========================================================================

static void decode_rows_range(u8 from, u8 to) {
    for (u8 r = from; r < to && r < level_h; r++)
        decode_row(r);
}

// =========================================================================
// Load level entities from ROM
// =========================================================================

static void load_entities(void) {
    const LevelInfo *li = &levels[current_level];

    level_entity_count = li->entity_count;
    if (level_entity_count > MAX_LEVEL_ENTITIES)
        level_entity_count = MAX_LEVEL_ENTITIES;

    const u8 *ent = li->entities;
    for (u8 i = 0; i < level_entity_count; i++) {
        level_entities[i].tx = ent[i * 4];
        level_entities[i].ty = ent[i * 4 + 1];
        level_entities[i].type = ent[i * 4 + 2];
        level_entities[i].vx = (s8)ent[i * 4 + 3];
        level_entities[i].alive = 1;
    }
}

// =========================================================================
// Initialize level
// =========================================================================

void init_level(void) {
    const LevelInfo *li = &levels[current_level];
    level_w = li->width;
    level_h = li->height;

    // Invalidate decode cache
    memset(decode_valid, 0, sizeof(decode_valid));

    // Load entities
    load_entities();
    active_enemy_count = 0;
    miner_active = 0;

    // Find player start entity
    player_px = 40;  // default
    player_py = 40;
    for (u8 i = 0; i < level_entity_count; i++) {
        if (level_entities[i].type == 0) { // ENT_PLAYER_START
            player_px = (s16)level_entities[i].tx * 8 + 4;
            player_py = (s16)level_entities[i].ty * 8 + 4;
            break;
        }
    }

    // Find miner
    miner_active = 0;
    for (u8 i = 0; i < level_entity_count; i++) {
        if (level_entities[i].type == 4) { // ENT_MINER
            miner_px = (s16)level_entities[i].tx * 8 + 4;
            miner_py = (s16)level_entities[i].ty * 8 + 4;
            miner_active = 1;
            break;
        }
    }

    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
    player_on_ground = 0;
    player_thrusting = 0;
    laser_active = 0;
    dyn_active = 0;
    dyn_exploding = 0;
    anim_tick = 0;

    // Pre-decode visible rows around player
    {
        u8 start_ty = player_py >> 3;
        u8 from = (start_ty > 10) ? start_ty - 10 : 0;
        u8 to = start_ty + 22;
        if (to > level_h) to = level_h;
        decode_rows_range(from, to);
    }
}

void start_new_game(void) {
    score = 0;
    player_lives = START_LIVES;
    player_fuel = START_FUEL;
    player_dynamite = START_DYNAMITE;
    current_level = 0;
    init_level();
    game_state = STATE_LEVEL_INTRO;
    level_msg_timer = LEVEL_INTRO_TIME;
}
