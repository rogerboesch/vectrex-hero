/*
 * game.h -- Mega Drive R.E.S.C.U.E. shared defines, types, externs
 *
 * 320x224 display, dual-plane scrolling, 60fps NTSC / 50fps PAL.
 */

#ifndef RESCUE_H
#define RESCUE_H

#include <genesis.h>

/* =========================================================================
 * Game constants (tuned for 60fps NTSC)
 * ========================================================================= */

#define GRAVITY         1
#define THRUST          2
#define MAX_VEL_Y       3
#define MAX_VEL_X       2
#define WALK_SPEED      1
#define FUEL_DRAIN      1

#define START_FUEL      255
#define START_LIVES       3
#define START_DYNAMITE    3

#define LASER_LENGTH     48
#define LASER_LIFETIME   10
#define EXPLOSION_RADIUS 24
#define EXPLOSION_KILL   10
#define EXPLOSION_TIME   20
#define DYNAMITE_FUSE    60
#define DEATH_TIME       30
#define DEATH_ANIM_TIME  30
#define RESCUE_ANIM_TIME 60

#define SPIDER_PATROL    16
#define SPIDER_HW         4
#define SPIDER_HH         4
#define SNAKE_HW          4
#define SNAKE_HH          3
#define BAT_HW            4
#define BAT_HH            4
#define MINER_HW          5
#define MINER_HH          5

/* Player hitbox */
#define PLAYER_HW  3
#define PLAYER_HH  7

#define LEVEL_INTRO_TIME 60

#define MAX_ACTIVE_ENEMIES  8
#define MAX_LEVEL_ENTITIES 32

/* =========================================================================
 * Joystick flags (matching GBC layout for code compatibility)
 * ========================================================================= */

#define J_RIGHT  BUTTON_RIGHT
#define J_LEFT   BUTTON_LEFT
#define J_UP     BUTTON_UP
#define J_DOWN   BUTTON_DOWN
#define J_A      BUTTON_A
#define J_B      BUTTON_B
#define J_START  BUTTON_START
#define J_SELECT BUTTON_C

/* =========================================================================
 * Screen layout
 * ========================================================================= */

/* VDP: 320x224 = 40x28 tiles */
#define SCREEN_W     320
#define SCREEN_H     224
#define HUD_H         16    /* 2 tile rows for HUD */
#define PLAY_H       208    /* 26 tile rows for gameplay */
#define PLAY_COLS     40
#define PLAY_ROWS     26
#define HUD_ROWS       2

/* VDP plane size (64x32 tiles for scrolling ring buffer) */
#define PLANE_W       64
#define PLANE_H       32

/* Activate enemies within this margin of the viewport */
#define ACTIVATE_MARGIN 32

/* =========================================================================
 * Game states
 * ========================================================================= */

#define STATE_TITLE          0
#define STATE_LEVEL_INTRO    1
#define STATE_PLAYING        2
#define STATE_DYING          3
#define STATE_LEVEL_COMPLETE 4
#define STATE_GAME_OVER      5
#define STATE_RESCUED        6
#define STATE_LEVEL_FAILED   7

/* =========================================================================
 * Types
 * ========================================================================= */

typedef struct {
    s16 px, py;
    s16 home_py;
    u8 alive;
    s8 vx;
    u8 type;
    u8 anim;
    u8 ent_idx;
} ActiveEnemy;

#define ENEMY_BAT    1
#define ENEMY_SPIDER 2
#define ENEMY_SNAKE  3
#define ENEMY_MINER  4

typedef struct {
    u8 tx, ty;
    u8 type;
    s8 vx;
    u8 alive;
} LevelEntity;

/* =========================================================================
 * Level data
 * ========================================================================= */

#include "level_data.h"

#define DECODE_ROWS  32
#define DECODE_MAX_W 128

/* =========================================================================
 * Globals
 * ========================================================================= */

extern s16 player_px, player_py;
extern s8 player_vx, player_vy;
extern s8 player_facing;
extern u8 player_fuel;
extern u8 player_dynamite;
extern u8 player_lives;
extern u8 player_on_ground;
extern u8 player_thrusting;
extern u8 anim_tick;

extern s16 score;

extern u8 game_state;
extern u8 current_level;
extern u8 death_timer;
extern u8 level_msg_timer;

extern s16 cam_x, cam_y;
extern u8 cam_tx, cam_ty;

extern ActiveEnemy active_enemies[MAX_ACTIVE_ENEMIES];
extern u8 active_enemy_count;

extern s16 miner_px, miner_py;
extern u8 miner_active;

extern u8 laser_active;
extern s16 laser_px, laser_py;
extern s8 laser_dir;
extern u8 laser_timer;

extern u8 dyn_active;
extern s16 dyn_px, dyn_py;
extern u8 dyn_timer;
extern u8 dyn_exploding;
extern u8 dyn_expl_timer;

extern u16 joy;
extern u16 joy_pressed;

extern u8 level_w, level_h;
extern u8 decode_cache[DECODE_ROWS][DECODE_MAX_W];
extern LevelEntity level_entities[MAX_LEVEL_ENTITIES];
extern u8 level_entity_count;

/* =========================================================================
 * Prototypes
 * ========================================================================= */

/* main.c */
u8 box_overlap(s16 ax, s16 ay, s8 ahw, s8 ahh,
               s16 bx, s16 by, s8 bhw, s8 bhh);

/* levels.c */
void init_level(void);
void start_new_game(void);
void decode_row(u8 row);
u8 tile_at(u8 tx, u8 ty);
u8 tile_solid(u8 tx, u8 ty);

/* tiles.c */
void tiles_init(void);

/* render.c */
void render_init_level(void);
void render_update_camera(void);
void render_update_sprites(void);
void render_update_hud(void);
void render_hide_sprites(void);
void render_title(void);
void render_msg(const char *line1, const char *line2);
void render_clear_tile(u8 tx, u8 ty);

/* player.c */
void update_player_physics(void);
void handle_input(void);

/* enemies.c */
void fire_laser(void);
void update_laser(void);
void place_dynamite(void);
void update_dynamite(void);
void activate_nearby_enemies(void);
void update_active_enemies(void);
void check_miner_rescue(void);

#endif
