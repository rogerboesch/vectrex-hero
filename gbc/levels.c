//
// levels.c — Level management (GBC port)
//

#include "game.h"
#include "../src/levels.h"

const int8_t *room_cave_lines[MAX_ROOMS];
uint8_t room_has_miner[MAX_ROOMS];
uint8_t room_has_lava[MAX_ROOMS];
const int8_t *room_walls[MAX_ROOMS];
uint8_t room_wall_counts[MAX_ROOMS];
const int8_t *room_enemies_data[MAX_ROOMS];
uint8_t room_enemy_counts[MAX_ROOMS];
int8_t room_starts[MAX_ROOMS * 2];
int8_t room_miners[MAX_ROOMS * 2];
uint8_t room_exits[MAX_ROOMS * 4];
static uint8_t num_rooms;

#define MAX_CAVE_SEGS 34
static int8_t cave_seg_buf[MAX_CAVE_SEGS * 4];

static void load_level(uint8_t nr,
                       const int8_t * const *caves,
                       const uint8_t *has_miner,
                       const uint8_t *has_lava,
                       const int8_t * const *walls,
                       const uint8_t *wall_cnts,
                       const int8_t * const *enemies_arr,
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
        room_enemies_data[i] = enemies_arr[i];
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
    switch (current_level) {
        case 0: load_level(2, l1_room_caves, l1_room_has_miner, l1_room_has_lava, l1_room_walls, l1_room_wall_counts, l1_room_enemies, l1_room_enemy_counts, l1_room_starts, l1_room_miners, l1_room_exits); break;
        case 1: load_level(4, l2_room_caves, l2_room_has_miner, l2_room_has_lava, l2_room_walls, l2_room_wall_counts, l2_room_enemies, l2_room_enemy_counts, l2_room_starts, l2_room_miners, l2_room_exits); break;
        case 2: load_level(6, l3_room_caves, l3_room_has_miner, l3_room_has_lava, l3_room_walls, l3_room_wall_counts, l3_room_enemies, l3_room_enemy_counts, l3_room_starts, l3_room_miners, l3_room_exits); break;
        case 3: load_level(8, l4_room_caves, l4_room_has_miner, l4_room_has_lava, l4_room_walls, l4_room_wall_counts, l4_room_enemies, l4_room_enemy_counts, l4_room_starts, l4_room_miners, l4_room_exits); break;
        case 4: load_level(8, l5_room_caves, l5_room_has_miner, l5_room_has_lava, l5_room_walls, l5_room_wall_counts, l5_room_enemies, l5_room_enemy_counts, l5_room_starts, l5_room_miners, l5_room_exits); break;
        case 5: load_level(10, l6_room_caves, l6_room_has_miner, l6_room_has_lava, l6_room_walls, l6_room_wall_counts, l6_room_enemies, l6_room_enemy_counts, l6_room_starts, l6_room_miners, l6_room_exits); break;
        case 6: load_level(12, l7_room_caves, l7_room_has_miner, l7_room_has_lava, l7_room_walls, l7_room_wall_counts, l7_room_enemies, l7_room_enemy_counts, l7_room_starts, l7_room_miners, l7_room_exits); break;
        case 7: load_level(14, l8_room_caves, l8_room_has_miner, l8_room_has_lava, l8_room_walls, l8_room_wall_counts, l8_room_enemies, l8_room_enemy_counts, l8_room_starts, l8_room_miners, l8_room_exits); break;
        case 8: load_level(16, l9_room_caves, l9_room_has_miner, l9_room_has_lava, l9_room_walls, l9_room_wall_counts, l9_room_enemies, l9_room_enemy_counts, l9_room_starts, l9_room_miners, l9_room_exits); break;
        case 9: load_level(16, l10_room_caves, l10_room_has_miner, l10_room_has_lava, l10_room_walls, l10_room_wall_counts, l10_room_enemies, l10_room_enemy_counts, l10_room_starts, l10_room_miners, l10_room_exits); break;
        case 10: load_level(16, l11_room_caves, l11_room_has_miner, l11_room_has_lava, l11_room_walls, l11_room_wall_counts, l11_room_enemies, l11_room_enemy_counts, l11_room_starts, l11_room_miners, l11_room_exits); break;
        case 11: load_level(16, l12_room_caves, l12_room_has_miner, l12_room_has_lava, l12_room_walls, l12_room_wall_counts, l12_room_enemies, l12_room_enemy_counts, l12_room_starts, l12_room_miners, l12_room_exits); break;
        case 12: load_level(16, l13_room_caves, l13_room_has_miner, l13_room_has_lava, l13_room_walls, l13_room_wall_counts, l13_room_enemies, l13_room_enemy_counts, l13_room_starts, l13_room_miners, l13_room_exits); break;
        case 13: load_level(16, l14_room_caves, l14_room_has_miner, l14_room_has_lava, l14_room_walls, l14_room_wall_counts, l14_room_enemies, l14_room_enemy_counts, l14_room_starts, l14_room_miners, l14_room_exits); break;
        case 14: load_level(16, l15_room_caves, l15_room_has_miner, l15_room_has_lava, l15_room_walls, l15_room_wall_counts, l15_room_enemies, l15_room_enemy_counts, l15_room_starts, l15_room_miners, l15_room_exits); break;
        case 15: load_level(16, l16_room_caves, l16_room_has_miner, l16_room_has_lava, l16_room_walls, l16_room_wall_counts, l16_room_enemies, l16_room_enemy_counts, l16_room_starts, l16_room_miners, l16_room_exits); break;
        case 16: load_level(16, l17_room_caves, l17_room_has_miner, l17_room_has_lava, l17_room_walls, l17_room_wall_counts, l17_room_enemies, l17_room_enemy_counts, l17_room_starts, l17_room_miners, l17_room_exits); break;
        case 17: load_level(16, l18_room_caves, l18_room_has_miner, l18_room_has_lava, l18_room_walls, l18_room_wall_counts, l18_room_enemies, l18_room_enemy_counts, l18_room_starts, l18_room_miners, l18_room_exits); break;
        case 18: load_level(16, l19_room_caves, l19_room_has_miner, l19_room_has_lava, l19_room_walls, l19_room_wall_counts, l19_room_enemies, l19_room_enemy_counts, l19_room_starts, l19_room_miners, l19_room_exits); break;
        case 19: load_level(16, l20_room_caves, l20_room_has_miner, l20_room_has_lava, l20_room_walls, l20_room_wall_counts, l20_room_enemies, l20_room_enemy_counts, l20_room_starts, l20_room_miners, l20_room_exits); break;
        default: load_level(2, l1_room_caves, l1_room_has_miner, l1_room_has_lava, l1_room_walls, l1_room_wall_counts, l1_room_enemies, l1_room_enemy_counts, l1_room_starts, l1_room_miners, l1_room_exits); break;
    }
    set_room_data();
}

void set_room_data(void) {
    const int8_t *p;
    uint8_t n, i, seg_idx;
    int8_t cx, cy;

    cur_cave_lines = room_cave_lines[current_room];

    p = cur_cave_lines;
    seg_idx = 0;
    while ((n = (uint8_t)*p++) != 0 && seg_idx < MAX_CAVE_SEGS) {
        cy = p[0]; cx = p[1]; p += 2;
        for (i = 0; i < n && seg_idx < MAX_CAVE_SEGS; i++) {
            cave_seg_buf[seg_idx * 4]     = cx;
            cave_seg_buf[seg_idx * 4 + 1] = cy;
            cy += p[0]; cx += p[1];
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
    cur_has_miner = room_has_miner[current_room];
    cur_has_lava = room_has_lava[current_room];
    cur_walls = room_walls[current_room];
    cur_wall_count = room_wall_counts[current_room];
    cur_enemies_data = room_enemies_data[current_room];
    cur_enemy_count = room_enemy_counts[current_room];
    cur_miner_x = room_miners[current_room * 2];
    cur_miner_y = room_miners[current_room * 2 + 1];
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
        } else {
            enemies[i].alive = 0;
        }
    }
    walls_destroyed = 0;
}

void init_level(void) {
    uint8_t j;
    current_room = 0;
    for (j = 0; j < MAX_ROOMS; j++) room_walls_destroyed[j] = 0;
    set_level_data();
    player_x = room_starts[0];
    player_y = room_starts[1];
    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
    player_on_ground = 0;
    player_thrusting = 0;
    laser_active = 0;
    dyn_active = 0;
    dyn_exploding = 0;
    anim_tick = 0;
    load_enemies();
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
