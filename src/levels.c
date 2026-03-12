//
// levels.c — Level management
//

#include "hero.h"
#include "levels.h"

// Room table pointers (set by set_level_data based on current_level)
const int8_t *room_cave_lines[MAX_ROOMS];
const int8_t *room_cave_segs[MAX_ROOMS];
uint8_t room_seg_counts[MAX_ROOMS];
uint8_t room_has_miner[MAX_ROOMS];
uint8_t room_has_lava[MAX_ROOMS];
const int8_t *room_walls[MAX_ROOMS];
uint8_t room_wall_counts[MAX_ROOMS];
const int8_t *room_enemies_data[MAX_ROOMS];
uint8_t room_enemy_counts[MAX_ROOMS];
int8_t room_starts[MAX_ROOMS * 2];
int8_t room_miners[MAX_ROOMS * 2];
uint8_t room_exits[MAX_ROOMS * 4];
int8_t room_bounds[MAX_ROOMS * 4];
static uint8_t num_rooms;

void set_level_data(void) {
    uint8_t i;
    num_rooms = 2;
    for (i = 0; i < num_rooms; i++) {
        room_cave_lines[i] = l1_room_caves[i];
        room_cave_segs[i] = l1_room_cave_segs[i];
        room_seg_counts[i] = l1_room_seg_counts[i];
        room_has_miner[i] = l1_room_has_miner[i];
        room_has_lava[i] = l1_room_has_lava[i];
        room_walls[i] = l1_room_walls[i];
        room_wall_counts[i] = l1_room_wall_counts[i];
        room_enemies_data[i] = l1_room_enemies[i];
        room_enemy_counts[i] = l1_room_enemy_counts[i];
        room_starts[i * 2] = l1_room_starts[i * 2];
        room_starts[i * 2 + 1] = l1_room_starts[i * 2 + 1];
        room_miners[i * 2] = l1_room_miners[i * 2];
        room_miners[i * 2 + 1] = l1_room_miners[i * 2 + 1];
        room_exits[i * 4] = l1_room_exits[i * 4];
        room_exits[i * 4 + 1] = l1_room_exits[i * 4 + 1];
        room_exits[i * 4 + 2] = l1_room_exits[i * 4 + 2];
        room_exits[i * 4 + 3] = l1_room_exits[i * 4 + 3];
        room_bounds[i * 4] = l1_room_bounds[i * 4];
        room_bounds[i * 4 + 1] = l1_room_bounds[i * 4 + 1];
        room_bounds[i * 4 + 2] = l1_room_bounds[i * 4 + 2];
        room_bounds[i * 4 + 3] = l1_room_bounds[i * 4 + 3];
    }
    set_room_data();
}

void set_room_data(void) {
    cur_cave_lines = room_cave_lines[current_room];
    cur_cave_segs = room_cave_segs[current_room];
    cur_seg_count = room_seg_counts[current_room];
    cur_cave_left = room_bounds[current_room * 4];
    cur_cave_right = room_bounds[current_room * 4 + 1];
    cur_cave_top = room_bounds[current_room * 4 + 2];
    cur_cave_floor = room_bounds[current_room * 4 + 3];
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
            enemies[i].x = cur_enemies_data[i * 3];
            enemies[i].y = cur_enemies_data[i * 3 + 1];
            enemies[i].vx = cur_enemies_data[i * 3 + 2];
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
    level_msg_timer = 30;
}

void start_new_game(void) {
    score = 0;
    player_lives = START_LIVES;
    player_fuel = START_FUEL;
    player_dynamite = START_DYNAMITE;
    current_level = 0;
    init_level();
    game_state = STATE_PLAYING;
}
