/*
 * main.c — C64 R.E.S.C.U.E. entry point and game loop
 */

#include "game.h"
#include "tiles.h"

/* =========================================================================
 * Global variables
 * ========================================================================= */

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

/* =========================================================================
 * Collision helper
 * ========================================================================= */

uint8_t box_overlap(int16_t ax, int16_t ay, int8_t ahw, int8_t ahh,
                    int16_t bx, int16_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

/* =========================================================================
 * Raster wait (sync to frame)
 * ========================================================================= */

static void wait_frame(void) {
    /* Wait for raster line 251 (bottom of visible area) */
    while ((*(uint8_t*)0xD012) != 251)
        ;
    /* Wait for it to pass */
    while ((*(uint8_t*)0xD012) == 251)
        ;
}

/* =========================================================================
 * Input (joystick port 2)
 * ========================================================================= */

#define JOY_UP    0x01
#define JOY_DOWN  0x02
#define JOY_LEFT  0x04
#define JOY_RIGHT 0x08
#define JOY_FIRE  0x10

static void read_input(void) {
    uint8_t raw = ~JOY2 & 0x1F;  /* active low, invert */
    joy_prev = joy;

    joy = 0;
    if (raw & JOY_UP)    joy |= 0x04;  /* map to common flags */
    if (raw & JOY_DOWN)  joy |= 0x08;
    if (raw & JOY_LEFT)  joy |= 0x02;
    if (raw & JOY_RIGHT) joy |= 0x01;
    if (raw & JOY_FIRE)  joy |= 0x10;

    joy_pressed = joy & ~joy_prev;
}

/* Common joy flags (matching GBC J_* layout) */
#define J_RIGHT  0x01
#define J_LEFT   0x02
#define J_UP     0x04
#define J_DOWN   0x08
#define J_A      0x10  /* fire button */
#define J_B      0x10  /* same button on C64 */
#define J_START  0x10  /* fire also starts game */
#define J_SELECT 0x00  /* no select on C64 */

/* =========================================================================
 * Main
 * ========================================================================= */

void main(void) {
    tiles_init();

    /* Set colors */
    VIC_BORDER = COLOR_BLACK;
    VIC_BG0 = COLOR_BLACK;

    game_state = STATE_TITLE;
    render_title();

    while (1) {
        wait_frame();
        read_input();

        if (game_state == STATE_TITLE) {
            anim_tick++;
            if (joy_pressed & (J_START | J_A)) {
                start_new_game();
                render_msg("LEVEL 01", 0);
            }
        }
        else if (game_state == STATE_LEVEL_INTRO) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_PLAYING;
                render_init_level();
            }
        }
        else if (game_state == STATE_PLAYING) {
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
        }
        else if (game_state == STATE_DYING) {
            death_timer--;
            render_update_sprites();

            if (death_timer == 0) {
                player_lives--;
                render_hide_sprites();
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                    render_msg("GAME OVER", 0);
                }
                else {
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                    game_state = STATE_LEVEL_FAILED;
                    level_msg_timer = LEVEL_INTRO_TIME;
                    render_msg("LEVEL FAILED", 0);
                }
            }
        }
        else if (game_state == STATE_LEVEL_COMPLETE) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
                level_msg_timer = LEVEL_INTRO_TIME;
                render_hide_sprites();
                render_msg("MINER RESCUED", 0);
            }
        }
        else if (game_state == STATE_RESCUED) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                current_level++;
                if (current_level >= NUM_LEVELS) current_level = 0;
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
                render_msg("LEVEL", 0);
            }
        }
        else if (game_state == STATE_LEVEL_FAILED) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                init_level();
                game_state = STATE_LEVEL_INTRO;
                level_msg_timer = LEVEL_INTRO_TIME;
                render_msg("LEVEL", 0);
            }
        }
        else if (game_state == STATE_GAME_OVER) {
            if (joy_pressed & (J_START | J_A)) {
                game_state = STATE_TITLE;
                render_title();
            }
        }
    }
}
