/*
 * main.c -- Mega Drive R.E.S.C.U.E. entry point and game loop
 */

#include "game.h"
#include "tiles.h"

/* =========================================================================
 * Global variables
 * ========================================================================= */

s16 player_px, player_py;
s8 player_vx, player_vy;
s8 player_facing;
u8 player_fuel;
u8 player_dynamite;
u8 player_lives;
u8 player_on_ground;
u8 player_thrusting;
u8 anim_tick;

s16 score;

u8 game_state;
u8 current_level;
u8 death_timer;
u8 level_msg_timer;

s16 cam_x, cam_y;
u8 cam_tx, cam_ty;

ActiveEnemy active_enemies[MAX_ACTIVE_ENEMIES];
u8 active_enemy_count;

s16 miner_px, miner_py;
u8 miner_active;

u8 laser_active;
s16 laser_px, laser_py;
s8 laser_dir;
u8 laser_timer;

u8 dyn_active;
s16 dyn_px, dyn_py;
u8 dyn_timer;
u8 dyn_exploding;
u8 dyn_expl_timer;

u16 joy;
u16 joy_pressed;
static u16 joy_prev;

/* =========================================================================
 * Collision helper
 * ========================================================================= */

u8 box_overlap(s16 ax, s16 ay, s8 ahw, s8 ahh,
               s16 bx, s16 by, s8 bhw, s8 bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

/* =========================================================================
 * Input (SGDK joypad)
 * ========================================================================= */

static void read_input(void) {
    u16 state;

    joy_prev = joy;
    state = JOY_readJoypad(JOY_1);
    joy = state;
    joy_pressed = joy & ~joy_prev;
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    /* SGDK initializes VDP, interrupts, etc. */

    tiles_init();

    VDP_setBackgroundColor(0);  /* black */

    game_state = STATE_TITLE;
    render_title();

    while (1) {
        SYS_doVBlankProcess();
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
            /* Minimal game loop for testing — just update HUD */
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

    return 0;
}
