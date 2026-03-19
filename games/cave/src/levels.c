//
// levels.c — Cave Diver level management
//

#include "cave.h"
#include "levels.h"

static uint8_t num_rooms;

// Collision segments computed at runtime from cave_lines draw data
#define MAX_CAVE_SEGS 34
static int8_t cave_seg_buf[MAX_CAVE_SEGS * 4];

// ---- Hardcoded test level (1 room) until levels.h is generated ----

// Simple open room cave shape
static const int8_t test_cave[] = {
    5, 50, -90,      // 5 segments, start at y=50, x=-90
    0, 180,           // right
    -100, 0,          // down
    0, -180,          // left
    100, 0,           // up
    0, 0,             // close
    0                 // terminator
};

static const int8_t test_walls[] = { 0, 0, 0, 0 };
static const int8_t test_enemies[] = {
    -40, 9, 1, 0     // fish at (-40, 9), vx=1, type=ENEMY_FISH
};

static const int8_t *test_caves[] = { test_cave };
static const uint8_t test_has_miner[] = { 0 };
static const uint8_t test_has_lava[] = { 0 };
static const int8_t *test_wall_ptrs[] = { test_walls };
static const uint8_t test_wall_cnts[] = { 0 };
static const int8_t *test_enemy_ptrs[] = { test_enemies };
static const uint8_t test_enemy_cnts[] = { 1 };
static const int8_t test_starts[] = { -66, 40 };
static const int8_t test_miners[] = { 0, 0 };
static const uint8_t test_exits[] = { 255, 255, 255, 255 };

// ---- Level loading (same pattern as hero) ----

static void load_level(uint8_t nr,
                       const int8_t * const *caves,
                       const uint8_t *has_miner,
                       const uint8_t *has_lava,
                       const int8_t * const *walls,
                       const uint8_t *wall_cnts,
                       const int8_t * const *enemies,
                       const uint8_t *enemy_cnts,
                       const int8_t *starts,
                       const int8_t *miners,
                       const uint8_t *exits) {
    uint8_t i;
    num_rooms = nr;
    for (i = 0; i < nr; i++) {
        room_cave_lines[i] = caves[i];
        room_has_miner[i] = has_miner[i];
        room_has_lava[i] = has_lava[i];
        room_walls[i] = walls[i];
        room_wall_counts[i] = wall_cnts[i];
        room_enemies_data[i] = enemies[i];
        room_enemy_counts[i] = enemy_cnts[i];
        room_starts[i * 2] = starts[i * 2];
        room_starts[i * 2 + 1] = starts[i * 2 + 1];
        room_miners[i * 2] = miners[i * 2];
        room_miners[i * 2 + 1] = miners[i * 2 + 1];
        room_exits[i * 4] = exits[i * 4];
        room_exits[i * 4 + 1] = exits[i * 4 + 1];
        room_exits[i * 4 + 2] = exits[i * 4 + 2];
        room_exits[i * 4 + 3] = exits[i * 4 + 3];
    }
}

void set_level_data(void) {
    // For now, always load the test level
    load_level(1, test_caves, test_has_miner, test_has_lava,
               test_wall_ptrs, test_wall_cnts,
               test_enemy_ptrs, test_enemy_cnts,
               test_starts, test_miners, test_exits);
    set_room_data();
}

void set_room_data(void) {
    const int8_t *p;
    uint8_t n, i, seg_idx;
    int8_t cx, cy;

    cur_cave_lines = room_cave_lines[current_room];

    // Compute collision segments from cave_lines draw data
    p = cur_cave_lines;
    seg_idx = 0;
    while ((n = (uint8_t)*p++) != 0 && seg_idx < MAX_CAVE_SEGS) {
        cy = p[0];
        cx = p[1];
        p += 2;
        for (i = 0; i < n && seg_idx < MAX_CAVE_SEGS; i++) {
            cave_seg_buf[seg_idx * 4]     = cx;
            cave_seg_buf[seg_idx * 4 + 1] = cy;
            cy += p[0];
            cx += p[1];
            cave_seg_buf[seg_idx * 4 + 2] = cx;
            cave_seg_buf[seg_idx * 4 + 3] = cy;
            p += 2;
            seg_idx++;
        }
    }
    cur_cave_segs = cave_seg_buf;
    cur_seg_count = seg_idx;

    cur_cave_left = ROOM_BOUND_LEFT;
    cur_cave_right = ROOM_BOUND_RIGHT;
    cur_cave_top = ROOM_BOUND_TOP;
    cur_cave_floor = ROOM_BOUND_FLOOR;
    cur_has_lava = room_has_lava[current_room];
    cur_walls = room_walls[current_room];
    cur_wall_count = room_wall_counts[current_room];
    cur_enemies_data = room_enemies_data[current_room];
    cur_enemy_count = room_enemy_counts[current_room];
    cur_has_diver = room_has_miner[current_room];
    cur_diver_x = room_miners[current_room * 2];
    cur_diver_y = room_miners[current_room * 2 + 1];
}

void load_enemies(void) {
    uint8_t i;
    enemy_count = cur_enemy_count;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (i < enemy_count) {
            enemies[i].x = cur_enemies_data[i * 4];
            enemies[i].y = cur_enemies_data[i * 4 + 1];
            enemies[i].vx = cur_enemies_data[i * 4 + 2];
            enemies[i].type = (uint8_t)cur_enemies_data[i * 4 + 3];
            enemies[i].home_y = enemies[i].y;
            enemies[i].alive = 1;
            enemies[i].anim = 0;
        }
        else {
            enemies[i].alive = 0;
        }
    }
    walls_destroyed = 0;
}

void init_level(void) {
    uint8_t j;
    current_room = 0;
    player_oxygen = START_OXYGEN;
    sonar_active = 0;
    for (j = 0; j < MAX_ROOMS; j++) room_walls_destroyed[j] = 0;
    set_level_data();
    player_x = room_starts[0];
    player_y = room_starts[1];
    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
    player_on_ground = 0;
    player_swimming = 0;
    anim_tick = 0;
    load_enemies();
}

void start_new_game(void) {
    score = 0;
    player_lives = START_LIVES;
    player_oxygen = START_OXYGEN;
#ifdef START_LEVEL
    current_level = START_LEVEL;
#else
    current_level = 0;
#endif
    init_level();
    game_state = STATE_LEVEL_INTRO;
    level_msg_timer = LEVEL_INTRO_TIME;
}
