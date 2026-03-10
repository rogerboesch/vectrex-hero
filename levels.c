//
// levels.c — Level management
//

#include "hero.h"
#include "levels.h"

void set_level_data(void) {
    if (current_level == 0) {
        cur_walls = l1_walls;
        cur_wall_count = L1_WALL_COUNT;
        cur_enemies_data = l1_enemies;
        cur_enemy_count = L1_ENEMY_COUNT;
        cur_miner_x = 0;
        cur_miner_y = CAVE_FLOOR + 8;
    } else {
        cur_walls = l2_walls;
        cur_wall_count = L2_WALL_COUNT;
        cur_enemies_data = l2_enemies;
        cur_enemy_count = L2_ENEMY_COUNT;
        cur_miner_x = 0;
        cur_miner_y = CAVE_FLOOR + 8;
    }
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
    set_level_data();
    player_x = CAVE_LEFT + 15;
    player_y = CAVE_TOP - 2;
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
