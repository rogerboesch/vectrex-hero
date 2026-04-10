//
// R.E.S.C.U.E. for Vectrex
// A conversion of Activision's H.E.R.O. (1984)
// Built with VectreC toolchain + CMOC
//

#include "hero.h"
#include "sprites.h"
#include "debug.h"

#ifndef EDITOR_TEST
#pragma vx_copyright "2026"
#pragma vx_title_pos 0,-80
#pragma vx_title_size -8, 80
#pragma vx_title "g RESCUE"
#pragma vx_music vx_music_1
#else
#pragma vx_copyright "2026"
#pragma vx_title "LEVEL TEST"
#pragma vx_music vx_music_1
#endif

// =========================================================================
// Global variable definitions
// =========================================================================

int8_t player_x;
int8_t player_y;
int8_t player_vx;
int8_t player_vy;
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

// Enemies
Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;

// Laser
uint8_t laser_active;
int8_t laser_x;
int8_t laser_y;
int8_t laser_dir;
uint8_t laser_timer;

// Dynamite
uint8_t dyn_active;
int8_t dyn_x;
int8_t dyn_y;
uint8_t dyn_timer;
uint8_t dyn_exploding;
uint8_t dyn_expl_timer;

// Destroyed walls (bitfield)
uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS];

// Current level pointers
const int8_t *cur_cave_lines;
int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
const int8_t *cur_walls;
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;
uint8_t cur_enemy_count;
int8_t *cur_cave_segs;
uint8_t cur_seg_count;
int8_t cur_miner_x;
int8_t cur_miner_y;
uint8_t cur_has_miner;
uint8_t cur_has_lava;

char str_buf[16];

// =========================================================================
// Helper functions
// =========================================================================

uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

// Get wall data from flat array: each wall is 4 bytes (y, x, h, w)
int8_t wall_y(uint8_t i)  { return cur_walls[i * 4]; }
int8_t wall_x(uint8_t i)  { return cur_walls[i * 4 + 1]; }
int8_t wall_h(uint8_t i)  { return cur_walls[i * 4 + 2]; }
int8_t wall_w(uint8_t i)  { return cur_walls[i * 4 + 3]; }

uint8_t player_hits_wall(uint8_t i) {
    int8_t wcx = wall_x(i) + (wall_w(i) / 2);
    int8_t wcy = wall_y(i);
    int8_t whw = wall_w(i) / 2;
    int8_t whh = wall_h(i);
    return box_overlap(player_x, player_y, SPRITE_HW(player_right), SPRITE_HH(player_right),
                       wcx, wcy, whw, whh);
}

// =========================================================================
// Initialization
// =========================================================================

void vectrex_init(void) {
    set_beam_intensity(INTENSITY_NORMAL);
    set_scale(0x7F);
    stop_music();
    stop_sound();
    controller_enable_1_x();
    controller_enable_1_y();
}

// =========================================================================
// Main game loop
// =========================================================================

int main(void) {
    vectrex_init();
#ifdef EDITOR_TEST
    start_new_game();
#else
    game_state = STATE_TITLE;
#endif

    while (TRUE) {
        BREAK(1);

        wait_recal();
        intensity_a(INTENSITY_NORMAL);
        controller_check_buttons();

#ifndef EDITOR_TEST
        if (game_state == STATE_TITLE) {
            anim_tick++;
            draw_title_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                start_new_game();
            }
        }
        else if (game_state == STATE_LEVEL_INTRO) {
            draw_level_intro_screen();
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
            }
        }
        else
#endif
        if (game_state == STATE_PLAYING) {
            anim_tick++;

            handle_input();
            update_player_physics();
            update_laser();
            update_dynamite();
            update_enemies();
            check_miner_rescue();

            // Lava death check
            if (cur_has_lava && player_y - SPRITE_HH(player_right) <= cur_cave_floor + LAVA_HEIGHT) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }

            // Check room exits using per-room cave bounds
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

            // Draw everything (minimal zero_beam calls)
            draw_cave();
            draw_lava();
            draw_enemies();
            draw_dynamite_and_explosion();
            draw_laser_beam();
            draw_player();
            draw_miner();
            draw_hud();
            draw_fuel_bar();
        }
        else if (game_state == STATE_DYING) {
            death_timer--;
            if (death_timer & 2) {
                draw_player();
            }
            draw_cave();
            draw_lava();

            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
#ifdef EDITOR_TEST
                    start_new_game();
#else
                    game_state = STATE_GAME_OVER;
#endif
                } else {
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
#ifdef EDITOR_TEST
                    init_level();
                    game_state = STATE_PLAYING;
#else
                    game_state = STATE_LEVEL_FAILED;
#endif
                }
            }
        }
        else if (game_state == STATE_LEVEL_COMPLETE) {
            draw_cave();
            draw_lava();
#ifdef EDITOR_TEST
            zero_beam();
            set_scale(0x7F);
            draw_text(100, -35, "RESCUED", 0x50, 10);
#endif

            level_msg_timer--;
            if (level_msg_timer == 0) {
#ifdef EDITOR_TEST
                init_level();
                game_state = STATE_PLAYING;
#else
                game_state = STATE_RESCUED;
#endif
            }
        }
#ifndef EDITOR_TEST
        else if (game_state == STATE_RESCUED) {
            draw_rescued_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                current_level++;
                if (current_level >= NUM_LEVELS) {
                    current_level = 0;
                }
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
            }
        }
        else if (game_state == STATE_LEVEL_FAILED) {
            draw_failed_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
            }
        }
#endif
        else if (game_state == STATE_GAME_OVER) {
#ifdef EDITOR_TEST
            start_new_game();
#else
            draw_game_over_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                game_state = STATE_TITLE;
            }
#endif
        }
    }

    return 0;
}
