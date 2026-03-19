//
// Cave Diver for Vectrex
// An underwater cave exploration game
//

#include "cave.h"
#include "sprites.h"

#pragma vx_copyright "2026"
#pragma vx_title_pos 0,-80
#pragma vx_title_size -8, 80
#pragma vx_title "g CAVE DIVER"
#pragma vx_music vx_music_1

// =========================================================================
// Global variable definitions
// =========================================================================

int8_t player_x;
int8_t player_y;
int8_t player_vx;
int8_t player_vy;
int8_t player_facing;
uint8_t player_oxygen;
uint8_t player_lives;
uint8_t player_on_ground;
uint8_t player_swimming;
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

// Sonar
uint8_t sonar_active;
uint8_t sonar_timer;

// Walls
uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS];

// Current room state
const int8_t *cur_cave_lines;
int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
const int8_t *cur_walls;
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;
uint8_t cur_enemy_count;
int8_t *cur_cave_segs;
uint8_t cur_seg_count;
int8_t cur_diver_x;
int8_t cur_diver_y;
uint8_t cur_has_diver;
uint8_t cur_has_lava;

// Room tables
const int8_t *room_cave_lines[MAX_ROOMS];
const int8_t *room_walls[MAX_ROOMS];
uint8_t room_wall_counts[MAX_ROOMS];
const int8_t *room_enemies_data[MAX_ROOMS];
uint8_t room_enemy_counts[MAX_ROOMS];
int8_t room_starts[MAX_ROOMS * 2];
int8_t room_miners[MAX_ROOMS * 2];
uint8_t room_exits[MAX_ROOMS * 4];
uint8_t room_has_miner[MAX_ROOMS];
uint8_t room_has_lava[MAX_ROOMS];

char str_buf[16];

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
    return box_overlap(player_x, player_y, 4, 6,
                       wall_x(i), wall_y(i), wall_w(i) / 2, wall_h(i) / 2);
}

void vectrex_init(void) {
    intensity_a(INTENSITY_NORMAL);
    set_scale(0x7F);
}

// =========================================================================
// Main game loop
// =========================================================================

int main(void) {
    vectrex_init();
    game_state = STATE_TITLE;

    while (TRUE) {
        wait_recal();
        intensity_a(INTENSITY_NORMAL);
        controller_check_buttons();
        anim_tick++;

        switch (game_state) {
        case STATE_TITLE:
            draw_title_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                start_new_game();
            }
            break;

        case STATE_LEVEL_INTRO:
            draw_level_intro_screen();
            if (level_msg_timer > 0) level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
            }
            break;

        case STATE_PLAYING:
            handle_input();
            update_player_physics();
            update_enemies();
            update_sonar();
            check_diver_rescue();

            // Oxygen drain
            if (player_oxygen > 0) {
                player_oxygen -= OXYGEN_DRAIN;
            }
            else {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }

            draw_cave();
            draw_lava();
            draw_enemies();
            draw_diver();
            draw_sonar_pulse();
            draw_player();
            draw_hud();
            draw_oxygen_bar();
            break;

        case STATE_DYING:
            draw_cave();
            if (death_timer > 0) death_timer--;
            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                    level_msg_timer = FAILED_DELAY_TIME;
                }
                else {
                    init_level();
                    game_state = STATE_PLAYING;
                }
            }
            break;

        case STATE_RESCUED:
            draw_rescued_screen();
            if (level_msg_timer > 0) level_msg_timer--;
            if (level_msg_timer == 0) {
                current_level++;
                if (current_level >= NUM_LEVELS) {
                    game_state = STATE_GAME_OVER;
                    level_msg_timer = FAILED_DELAY_TIME;
                }
                else {
                    game_state = STATE_LEVEL_INTRO;
                    level_msg_timer = LEVEL_INTRO_TIME;
                    set_level_data();
                    init_level();
                }
            }
            break;

        case STATE_GAME_OVER:
            draw_game_over_screen();
            if (level_msg_timer > 0) level_msg_timer--;
            if (level_msg_timer == 0 && controller_button_1_1_pressed()) {
                game_state = STATE_TITLE;
            }
            break;
        }
    }
    return 0;
}
