//
// levels.c — Level loading, RLE decode, tile access (tilemap version)
//

#include "game.h"
#include "tiles.h"

uint8_t level_w, level_h;
uint8_t decode_cache[DECODE_ROWS][DECODE_MAX_W];

LevelEntity level_entities[MAX_LEVEL_ENTITIES];
uint8_t level_entity_count;

static uint8_t decode_valid[DECODE_ROWS]; // which rows are decoded

// =========================================================================
// RLE row decoder: decode a single row from ROM into decode_cache
// =========================================================================

void decode_row(uint8_t row) {
    if (row >= level_h) return;
    if (decode_valid[row & (DECODE_ROWS - 1)] == row + 1) return; // already decoded

    const LevelInfo *li = &levels[current_level];
    

    uint16_t offset = li->row_offsets[row];
    const uint8_t *rle = li->rle + offset;
    uint8_t *dst = decode_cache[row & (DECODE_ROWS - 1)];
    uint8_t col = 0;

    while (col < level_w) {
        uint8_t run = *rle++;
        uint8_t tile = *rle++;
        while (run-- && col < level_w)
            dst[col++] = tile;
    }
    decode_valid[row & (DECODE_ROWS - 1)] = row + 1;
}

// =========================================================================
// Tile access (reads from decode cache)
// =========================================================================

uint8_t tile_at(uint8_t tx, uint8_t ty) {
    if (tx >= level_w || ty >= level_h) return 1; // solid out of bounds
    decode_row(ty);
    return decode_cache[ty & (DECODE_ROWS - 1)][tx];
}

uint8_t tile_solid(uint8_t tx, uint8_t ty) {
    uint8_t t = tile_at(tx, ty);
    if (t == 0) return 0;
    if (t >= TILE_DECOR_FIRST && t <= TILE_DECOR_LAST) return 0;
    return 1;
}

// =========================================================================
// Decode a range of rows (for initial screen fill)
// =========================================================================

static void decode_rows_range(uint8_t from, uint8_t to) {
    for (uint8_t r = from; r < to && r < level_h; r++)
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

    const uint8_t *ent = li->entities;
    for (uint8_t i = 0; i < level_entity_count; i++) {
        level_entities[i].tx = ent[i * 4];
        level_entities[i].ty = ent[i * 4 + 1];
        level_entities[i].type = ent[i * 4 + 2];
        level_entities[i].vx = (int8_t)ent[i * 4 + 3];
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
    for (uint8_t i = 0; i < level_entity_count; i++) {
        if (level_entities[i].type == 0) { // ENT_PLAYER_START
            player_px = (int16_t)level_entities[i].tx * 8 + 4;
            player_py = (int16_t)level_entities[i].ty * 8 + 4;
            break;
        }
    }

    // Find miner
    miner_active = 0;
    for (uint8_t i = 0; i < level_entity_count; i++) {
        if (level_entities[i].type == 4) { // ENT_MINER
            miner_px = (int16_t)level_entities[i].tx * 8 + 4;
            miner_py = (int16_t)level_entities[i].ty * 8 + 4;
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
        uint8_t start_ty = player_py >> 3;
        uint8_t from = (start_ty > 10) ? start_ty - 10 : 0;
        uint8_t to = start_ty + 22;
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
