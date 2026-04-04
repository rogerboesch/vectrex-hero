/*
 * main.c -- Apple IIe R.E.S.C.U.E. entry point and game loop
 *
 * This is the central game loop that drives the state machine.
 * All global game state lives here (player, enemies, laser, dynamite,
 * room data). The loop runs at ~10 FPS (no frame pacing -- the 6502
 * at 1 MHz is the natural limiter).
 *
 * Adapted from ql/src/main.c with Apple II hardware calls.
 */

#include "game.h"
#include "sprites.h"

/* ===================================================================
 * Global variables -- all game state
 * =================================================================== */

/* Player state */
int8_t player_x, player_y;
int8_t player_vx, player_vy;
int8_t player_facing;
uint8_t player_fuel;
uint8_t player_dynamite;
uint8_t player_lives;
uint8_t player_on_ground;
uint8_t player_thrusting;
uint8_t anim_tick;

int16_t score;

/* Game state machine */
uint8_t game_state;
uint8_t current_level;
uint8_t current_room;
uint8_t death_timer;
uint8_t level_msg_timer;

/* Enemies */
Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;

/* Laser */
uint8_t laser_active;
int8_t laser_x, laser_y, laser_dir;
uint8_t laser_timer;

/* Dynamite */
uint8_t dyn_active;
int8_t dyn_x, dyn_y;
uint8_t dyn_timer;
uint8_t dyn_exploding;
uint8_t dyn_expl_timer;

/* Walls */
uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS];

/* Current room data */
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

/* ===================================================================
 * Helpers
 * =================================================================== */

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

/* ===================================================================
 * Main game loop
 * =================================================================== */

int main(void) {
    a2_init();
    render_init();

    game_state = STATE_TITLE;
    render_title_screen();

    for (;;) {
        a2_read_keys();

        /* --- Title screen --- */
        if (game_state == STATE_TITLE) {
            anim_tick++;
            if (key_enter_pressed || key_space_pressed) {
                start_new_game();
                game_state = STATE_PLAYING;
                render_room();
            }
        }
        /* --- Level intro --- */
        else if (game_state == STATE_LEVEL_INTRO) {
            game_state = STATE_PLAYING;
            render_room();
        }
        /* --- Main gameplay --- */
        else if (game_state == STATE_PLAYING) {
            anim_tick++;

            handle_input();
            update_player_physics();
            update_laser();
            update_dynamite();
            update_enemies();
            check_miner_rescue();

            /* Lava death */
            if (cur_has_lava && player_y - PLAYER_HH <= cur_cave_floor + LAVA_HEIGHT) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }

            /* Room exits */
            {
                uint8_t exit_room = NONE;
                if (player_x <= ROOM_BOUND_LEFT &&
                    room_exits[current_room * 4 + 0] != NONE) {
                    exit_room = room_exits[current_room * 4 + 0];
                    player_x = ROOM_BOUND_RIGHT;
                }
                else if (player_x >= ROOM_BOUND_RIGHT &&
                           room_exits[current_room * 4 + 1] != NONE) {
                    exit_room = room_exits[current_room * 4 + 1];
                    player_x = ROOM_BOUND_LEFT;
                }
                else if (player_y >= ROOM_BOUND_TOP &&
                           room_exits[current_room * 4 + 2] != NONE) {
                    exit_room = room_exits[current_room * 4 + 2];
                    player_y = ROOM_BOUND_FLOOR;
                }
                else if (player_y <= ROOM_BOUND_FLOOR &&
                           room_exits[current_room * 4 + 3] != NONE) {
                    exit_room = room_exits[current_room * 4 + 3];
                    player_y = ROOM_BOUND_TOP;
                }
                if (exit_room != NONE) {
                    room_walls_destroyed[current_room] = walls_destroyed;
                    current_room = exit_room;
                    set_room_data();
                    load_enemies();
                    walls_destroyed = room_walls_destroyed[current_room];
                    render_room();
                }
            }

            render_frame();
            render_hud();
        }
        /* --- Death animation --- */
        else if (game_state == STATE_DYING) {
            death_timer--;
            render_frame();

            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                    render_game_over();
                }
                else {
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                    game_state = STATE_LEVEL_FAILED;
                    render_failed();
                }
            }
        }
        /* --- Level complete --- */
        else if (game_state == STATE_LEVEL_COMPLETE) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
                render_rescued();
            }
        }
        /* --- Rescued --- */
        else if (game_state == STATE_RESCUED) {
            if (key_enter_pressed || key_space_pressed) {
                current_level++;
                if (current_level >= NUM_LEVELS) current_level = 0;
                init_level();
                game_state = STATE_PLAYING;
                render_room();
            }
        }
        /* --- Level failed --- */
        else if (game_state == STATE_LEVEL_FAILED) {
            if (key_enter_pressed || key_space_pressed) {
                init_level();
                game_state = STATE_PLAYING;
                render_room();
            }
        }
        /* --- Game over --- */
        else if (game_state == STATE_GAME_OVER) {
            if (key_enter_pressed || key_space_pressed) {
                game_state = STATE_TITLE;
                render_title_screen();
            }
        }
    }

    return 0;
}
