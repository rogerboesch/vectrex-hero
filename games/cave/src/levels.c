//
// levels.c — Cave Diver level management
//

#include "cave.h"
#include "levels.h"

// Room table pointers
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

// TODO: implement level loading from levels.h data
// For now, stub functions that match the engine interface

void set_level_data(void) {
    // Load level tables based on current_level
    // Will be populated when levels.h is generated
}

void set_room_data(void) {
    // Set cur_* pointers from room tables
}

void load_enemies(void) {
    // Spawn enemies for current room
    enemy_count = 0;
}

void init_level(void) {
    uint8_t i;
    current_room = 0;
    player_oxygen = START_OXYGEN;
    sonar_active = 0;
    walls_destroyed = 0;
    for (i = 0; i < MAX_ROOMS; i++) {
        room_walls_destroyed[i] = 0;
    }
    set_room_data();
    load_enemies();

    // Set player start position
    player_x = room_starts[0];
    player_y = room_starts[1];
    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
}

void start_new_game(void) {
    current_level = 0;
    player_lives = START_LIVES;
    score = 0;
    set_level_data();
    init_level();
}
