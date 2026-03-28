//
// main.c — GBC H.E.R.O. entry point and game loop
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// Global variables
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

int16_t score;

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

uint8_t joy;
uint8_t joy_pressed;
static uint8_t joy_prev;

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
// Input
// =========================================================================

static void read_input(void) {
    joy_prev = joy;
    joy = joypad();
    joy_pressed = joy & ~joy_prev;  // newly pressed this frame
}

// =========================================================================
// Room transition
// =========================================================================

static void enter_room(void) {
    // Brief LCD off for full tilemap update
    DISPLAY_OFF;
    set_room_data();
    load_enemies();
    walls_destroyed = room_walls_destroyed[current_room];
    render_init_room();
    DISPLAY_ON;
    SHOW_BKG;
    SHOW_SPRITES;
}

// =========================================================================
// Score to string helper (for message screens)
// =========================================================================

static char score_buf[10];

static const char *score_str(void) {
    int16_t s = score;
    uint8_t i = 0;
    char tmp[6];
    uint8_t j;
    if (s == 0) { score_buf[0] = '0'; score_buf[1] = '\0'; return score_buf; }
    while (s > 0 && i < 6) { tmp[i++] = '0' + (s % 10); s /= 10; }
    for (j = 0; j < i; j++) score_buf[j] = tmp[i - 1 - j];
    score_buf[i] = '\0';
    return score_buf;
}

// =========================================================================
// Main
// =========================================================================

void main(void) {
    // Detect and enable CGB mode
    if (_cpu == CGB_TYPE) {
        cpu_fast();  // double speed mode
    }

    tiles_init();

    DISPLAY_ON;
    SHOW_BKG;
    SHOW_SPRITES;

    game_state = STATE_TITLE;
    render_title();

    while (1) {
        wait_vbl_done();
        read_input();

        if (game_state == STATE_TITLE) {
            anim_tick++;
            if (joy_pressed & (J_START | J_A | J_B)) {
                start_new_game();
                DISPLAY_OFF;
                render_init_room();
                DISPLAY_ON;
                SHOW_BKG;
                SHOW_SPRITES;
            }

        } else if (game_state == STATE_LEVEL_INTRO) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
                DISPLAY_OFF;
                render_init_room();
                DISPLAY_ON;
                SHOW_BKG;
                SHOW_SPRITES;
            }

        } else if (game_state == STATE_PLAYING) {
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
                if (player_x <= ROOM_BOUND_LEFT &&
                    room_exits[current_room * 4 + 0] != NONE) {
                    exit_room = room_exits[current_room * 4 + 0];
                    player_x = ROOM_BOUND_RIGHT;
                } else if (player_x >= ROOM_BOUND_RIGHT &&
                           room_exits[current_room * 4 + 1] != NONE) {
                    exit_room = room_exits[current_room * 4 + 1];
                    player_x = ROOM_BOUND_LEFT;
                } else if (player_y >= ROOM_BOUND_TOP &&
                           room_exits[current_room * 4 + 2] != NONE) {
                    exit_room = room_exits[current_room * 4 + 2];
                    player_y = ROOM_BOUND_FLOOR;
                } else if (player_y <= ROOM_BOUND_FLOOR &&
                           room_exits[current_room * 4 + 3] != NONE) {
                    exit_room = room_exits[current_room * 4 + 3];
                    player_y = ROOM_BOUND_TOP;
                }
                if (exit_room != NONE) {
                    room_walls_destroyed[current_room] = walls_destroyed;
                    current_room = exit_room;
                    enter_room();
                }
            }

            render_update_sprites();
            render_update_hud();

        } else if (game_state == STATE_DYING) {
            death_timer--;
            render_update_sprites();

            if (death_timer == 0) {
                player_lives--;
                render_hide_sprites();
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                    render_msg("GAME OVER", score_str());
                } else {
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                    game_state = STATE_LEVEL_FAILED;
                    render_msg("LEVEL FAILED", score_str());
                }
            }

        } else if (game_state == STATE_LEVEL_COMPLETE) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
                render_hide_sprites();
                render_msg("RESCUED", score_str());
            }

        } else if (game_state == STATE_RESCUED) {
            if (joy_pressed & (J_START | J_A | J_B)) {
                current_level++;
                if (current_level >= NUM_LEVELS) current_level = 0;
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
                {
                    char buf[10];
                    buf[0] = 'L'; buf[1] = 'E'; buf[2] = 'V'; buf[3] = 'E'; buf[4] = 'L'; buf[5] = ' ';
                    buf[6] = '0' + ((current_level + 1) / 10);
                    buf[7] = '0' + ((current_level + 1) % 10);
                    buf[8] = '\0';
                    render_msg(buf, 0);
                }
            }

        } else if (game_state == STATE_LEVEL_FAILED) {
            if (joy_pressed & (J_START | J_A | J_B)) {
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
                {
                    char buf[10];
                    buf[0] = 'L'; buf[1] = 'E'; buf[2] = 'V'; buf[3] = 'E'; buf[4] = 'L'; buf[5] = ' ';
                    buf[6] = '0' + ((current_level + 1) / 10);
                    buf[7] = '0' + ((current_level + 1) % 10);
                    buf[8] = '\0';
                    render_msg(buf, 0);
                }
            }

        } else if (game_state == STATE_GAME_OVER) {
            if (joy_pressed & (J_START | J_A | J_B)) {
                game_state = STATE_TITLE;
                render_hide_sprites();
                render_title();
            }
        }
    }
}
