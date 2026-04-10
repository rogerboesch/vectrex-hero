//
// main.c — Console R.E.S.C.U.E. entry point and game loop
//

#include "game.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

// =========================================================================
// Global variable definitions
// =========================================================================

int8_t player_x, player_y;
int8_t player_vx, player_vy;
int8_t player_facing;
uint8_t player_fuel;
uint8_t player_dynamite;
uint8_t player_lives;
uint8_t player_on_ground;
uint8_t player_thrusting;
uint8_t anim_tick;

int score;

uint8_t game_state;
uint8_t current_level;
uint8_t current_room;
uint8_t death_timer;
uint8_t level_msg_timer;

Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;

uint8_t laser_active;
int8_t laser_x, laser_y, laser_dir;
uint8_t laser_timer;

uint8_t dyn_active;
int8_t dyn_x, dyn_y;
uint8_t dyn_timer;
uint8_t dyn_exploding;
uint8_t dyn_expl_timer;

uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS];

const int8_t *cur_cave_lines;
int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
const int8_t *cur_walls;
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;
uint8_t cur_enemy_count;
int8_t *cur_cave_segs;
uint8_t cur_seg_count;
int8_t cur_miner_x, cur_miner_y;
uint8_t cur_has_miner;
uint8_t cur_has_lava;

// Input state
uint8_t key_left, key_right, key_up, key_down;
uint8_t key_space_pressed;
uint8_t key_d_pressed;
uint8_t key_enter_pressed;

// =========================================================================
// Helpers
// =========================================================================

uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

int8_t wall_y(uint8_t i) { return cur_walls[i * 4]; }
int8_t wall_x(uint8_t i) { return cur_walls[i * 4 + 1]; }
int8_t wall_h(uint8_t i) { return cur_walls[i * 4 + 2]; }
int8_t wall_w(uint8_t i) { return cur_walls[i * 4 + 3]; }

uint8_t player_hits_wall(uint8_t i) {
    int8_t wcx = wall_x(i) + (wall_w(i) / 2);
    int8_t wcy = wall_y(i);
    int8_t whw = wall_w(i) / 2;
    int8_t whh = wall_h(i);
    return box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                       wcx, wcy, whw, whh);
}

// =========================================================================
// Timing
// =========================================================================

#define FRAME_US 66666  // ~15 FPS

static long long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

// =========================================================================
// Input processing — read all pending keys, set state
// =========================================================================

static void process_input(void) {
    int key;

    // Clear single-shot keys
    key_space_pressed = 0;
    key_d_pressed = 0;
    key_enter_pressed = 0;
    key_left = key_right = key_up = key_down = 0;

    // Read all pending keys
    while ((key = term_read_key()) != KEY_NONE) {
        switch (key) {
            case KEY_LEFT:  key_left = 1; break;
            case KEY_RIGHT: key_right = 1; break;
            case KEY_UP:    key_up = 1; break;
            case KEY_DOWN:  key_down = 1; break;
            case KEY_SPACE: key_space_pressed = 1; key_enter_pressed = 1; break;
            case KEY_ENTER: key_enter_pressed = 1; break;
            case KEY_D:     key_d_pressed = 1; break;
            case KEY_Q:     term_cleanup(); exit(0); break;
            case KEY_R:
                if (game_state == STATE_PLAYING) {
                    init_level();
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                }
                break;
        }
    }
}

// =========================================================================
// Main game loop
// =========================================================================

int main(void) {
    long long frame_start, elapsed;

    term_init();
    game_state = STATE_TITLE;

    while (1) {
        frame_start = now_us();
        process_input();

        if (game_state == STATE_TITLE) {
            anim_tick++;
            render_title_screen();
            if (key_enter_pressed || key_space_pressed) {
                start_new_game();
            }
        }
        else if (game_state == STATE_LEVEL_INTRO) {
            render_level_intro_screen();
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
            }
        }
        else if (game_state == STATE_PLAYING) {
            anim_tick++;

            handle_input();
            update_player_physics();
            update_laser();
            update_dynamite();
            update_enemies();
            check_miner_rescue();

            // Lava death
            if (cur_has_lava && player_y - PLAYER_HH <= cur_cave_floor + LAVA_HEIGHT) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }

            // Room exits
            {
                uint8_t exit_room = NONE;
                if (player_x <= ROOM_BOUND_LEFT
                    && room_exits[current_room * 4 + 0] != NONE) {
                    exit_room = room_exits[current_room * 4 + 0];
                    player_x = ROOM_BOUND_RIGHT;
                } else if (player_x >= ROOM_BOUND_RIGHT
                    && room_exits[current_room * 4 + 1] != NONE) {
                    exit_room = room_exits[current_room * 4 + 1];
                    player_x = ROOM_BOUND_LEFT;
                } else if (player_y >= ROOM_BOUND_TOP
                    && room_exits[current_room * 4 + 2] != NONE) {
                    exit_room = room_exits[current_room * 4 + 2];
                    player_y = ROOM_BOUND_FLOOR;
                } else if (player_y <= ROOM_BOUND_FLOOR
                    && room_exits[current_room * 4 + 3] != NONE) {
                    exit_room = room_exits[current_room * 4 + 3];
                    player_y = ROOM_BOUND_TOP;
                }
                if (exit_room != NONE) {
                    room_walls_destroyed[current_room] = walls_destroyed;
                    current_room = exit_room;
                    set_room_data();
                    load_enemies();
                    walls_destroyed = room_walls_destroyed[current_room];
                }
            }

            render_game();
        }
        else if (game_state == STATE_DYING) {
            death_timer--;
            render_game();

            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                } else {
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                    game_state = STATE_LEVEL_FAILED;
                }
            }
        }
        else if (game_state == STATE_LEVEL_COMPLETE) {
            render_game();
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
            }
        }
        else if (game_state == STATE_RESCUED) {
            render_rescued_screen();
            if (key_enter_pressed || key_space_pressed) {
                current_level++;
                if (current_level >= NUM_LEVELS) current_level = 0;
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
            }
        }
        else if (game_state == STATE_LEVEL_FAILED) {
            render_failed_screen();
            if (key_enter_pressed || key_space_pressed) {
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
            }
        }
        else if (game_state == STATE_GAME_OVER) {
            render_game_over_screen();
            if (key_enter_pressed || key_space_pressed) {
                game_state = STATE_TITLE;
            }
        }

        render_flush();

        // Frame timing
        elapsed = now_us() - frame_start;
        if (elapsed < FRAME_US) {
            usleep((unsigned int)(FRAME_US - elapsed));
        }
    }

    term_cleanup();
    return 0;
}
