//
// main.c — GBC R.E.S.C.U.E. entry point and game loop (tilemap scrolling version)
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// Global variables
// =========================================================================

int16_t player_px, player_py;
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
uint8_t death_timer;
uint8_t level_msg_timer;

int16_t cam_x, cam_y;
uint8_t cam_tx, cam_ty;

ActiveEnemy active_enemies[MAX_ACTIVE_ENEMIES];
uint8_t active_enemy_count;

int16_t miner_px, miner_py;
uint8_t miner_active;

uint8_t laser_active;
int16_t laser_px, laser_py;
int8_t laser_dir;
uint8_t laser_timer;

uint8_t dyn_active;
int16_t dyn_px, dyn_py;
uint8_t dyn_timer;
uint8_t dyn_exploding;
uint8_t dyn_expl_timer;

uint8_t joy;
uint8_t joy_pressed;
static uint8_t joy_prev;

// =========================================================================
// Helpers
// =========================================================================

uint8_t box_overlap(int16_t ax, int16_t ay, int8_t ahw, int8_t ahh,
                    int16_t bx, int16_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

// =========================================================================
// Input
// =========================================================================

static void read_input(void) {
    joy_prev = joy;
    joy = joypad();
    joy_pressed = joy & ~joy_prev;
}

// =========================================================================
// Score to string
// =========================================================================

static char score_buf[16];

static const char *score_str(void) {
    int16_t s = score;
    uint8_t i = 0;
    char tmp[6];
    uint8_t j;
    score_buf[0] = 'S'; score_buf[1] = 'C'; score_buf[2] = 'O';
    score_buf[3] = 'R'; score_buf[4] = 'E'; score_buf[5] = ':';
    score_buf[6] = ' ';
    if (s == 0) { score_buf[7] = '0'; score_buf[8] = '\0'; return score_buf; }
    while (s > 0 && i < 6) { tmp[i++] = '0' + (s % 10); s /= 10; }
    for (j = 0; j < i; j++) score_buf[7 + j] = tmp[i - 1 - j];
    score_buf[7 + i] = '\0';
    return score_buf;
}

// =========================================================================
// Main
// =========================================================================

void main(void) {
    if (_cpu == CGB_TYPE) {
        cpu_fast();
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
                render_msg("LEVEL 01", 0);
            }

        } else if (game_state == STATE_LEVEL_INTRO) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
                DISPLAY_OFF;
                render_init_level();
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
            activate_nearby_enemies();
            update_active_enemies();
            check_miner_rescue();

            render_update_camera();
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
                    level_msg_timer = LEVEL_INTRO_TIME;
                    render_msg("LEVEL FAILED", score_str());
                }
            }

        } else if (game_state == STATE_LEVEL_COMPLETE) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
                level_msg_timer = LEVEL_INTRO_TIME;
                render_hide_sprites();
                render_msg("MINER RESCUED", score_str());
            }

        } else if (game_state == STATE_RESCUED) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
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
            level_msg_timer--;
            if (level_msg_timer == 0) {
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
