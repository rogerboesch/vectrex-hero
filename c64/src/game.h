/*
 * game.h — C64 R.E.S.C.U.E. shared defines, types, externs
 *
 * Tile-based scrolling using VIC-II character mode.
 * Physics tuned for ~50fps PAL / ~60fps NTSC.
 */

#ifndef RESCUE_H
#define RESCUE_H

#include <stdint.h>
#include <string.h>

#define TRUE  1
#define FALSE 0
#define NONE  255

/* =========================================================================
 * Game constants (tuned for C64 ~50fps PAL)
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

#define LASER_LENGTH     40
#define LASER_LIFETIME   10
#define EXPLOSION_RADIUS 24
#define EXPLOSION_KILL   10
#define EXPLOSION_TIME   20
#define RESCUE_ANIM_TIME 50
#define DEATH_TIME       30

#define SPIDER_PATROL    16
#define SPIDER_HW         4
#define SPIDER_HH         4
#define SNAKE_HW          4
#define SNAKE_HH          3
#define BAT_HW            4
#define BAT_HH            4

/* Player hitbox (half-width, half-height in pixels) */
#define PLAYER_HW  3
#define PLAYER_HH  7

#define LEVEL_INTRO_TIME 50  /* frames (~1 second at 50fps) */

#define MAX_ACTIVE_ENEMIES  3
#define MAX_LEVEL_ENTITIES 32

/* =========================================================================
 * Screen layout
 * ========================================================================= */

/* VIC-II: 40x25 chars = 320x200 pixels */
#define SCREEN_W     320
#define SCREEN_H     200
#define HUD_H         16   /* 2 char rows for HUD */
#define PLAY_H       184   /* 23 char rows visible for gameplay */
#define PLAY_COLS     40
#define PLAY_ROWS     23
#define HUD_ROWS       2

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
 * Sprite screen positioning
 * ========================================================================= */

/* C64 sprite coordinates: X offset = 24, Y offset = 50 */
#define SPR_SCR_X(wpx) ((uint16_t)(24 + (int16_t)(wpx) - cam_x))
#define SPR_SCR_Y(wpy) ((uint8_t)(50 + HUD_H + (int16_t)(wpy) - cam_y))

/* =========================================================================
 * Types
 * ========================================================================= */

typedef struct {
    int16_t px, py;
    int16_t home_py;
    uint8_t alive;
    int8_t vx;
    uint8_t type;
    uint8_t anim;
    uint8_t ent_idx;
} ActiveEnemy;

#define ENEMY_BAT    1
#define ENEMY_SPIDER 2
#define ENEMY_SNAKE  3
#define ENEMY_MINER  4

typedef struct {
    uint8_t tx, ty;
    uint8_t type;
    int8_t vx;
    uint8_t alive;
} LevelEntity;

/* =========================================================================
 * Level data
 * ========================================================================= */

#define NUM_LEVELS 20
#define DECODE_ROWS  32
#define DECODE_MAX_W 128

typedef struct {
    uint8_t width, height;
    const uint8_t *rle;
    const uint16_t *row_offsets;
    uint8_t entity_count;
    const uint8_t *entities;
} LevelInfo;

extern const LevelInfo levels[NUM_LEVELS];

/* =========================================================================
 * Globals
 * ========================================================================= */

extern int16_t player_px, player_py;
extern int8_t player_vx, player_vy;
extern int8_t player_facing;
extern uint8_t player_fuel;
extern uint8_t player_dynamite;
extern uint8_t player_lives;
extern uint8_t player_on_ground;
extern uint8_t player_thrusting;
extern uint8_t anim_tick;

extern int16_t score;

extern uint8_t game_state;
extern uint8_t current_level;
extern uint8_t death_timer;
extern uint8_t level_msg_timer;

extern int16_t cam_x, cam_y;
extern uint8_t cam_tx, cam_ty;

extern ActiveEnemy active_enemies[MAX_ACTIVE_ENEMIES];
extern uint8_t active_enemy_count;

extern int16_t miner_px, miner_py;
extern uint8_t miner_active;

extern uint8_t laser_active;
extern int16_t laser_px, laser_py;
extern int8_t laser_dir;
extern uint8_t laser_timer;

extern uint8_t dyn_active;
extern int16_t dyn_px, dyn_py;
extern uint8_t dyn_timer;
extern uint8_t dyn_exploding;
extern uint8_t dyn_expl_timer;

extern uint8_t joy;
extern uint8_t joy_pressed;

extern uint8_t level_w, level_h;
extern uint8_t decode_cache[DECODE_ROWS][DECODE_MAX_W];
extern LevelEntity level_entities[MAX_LEVEL_ENTITIES];
extern uint8_t level_entity_count;

/* =========================================================================
 * Prototypes
 * ========================================================================= */

/* main.c */
uint8_t box_overlap(int16_t ax, int16_t ay, int8_t ahw, int8_t ahh,
                    int16_t bx, int16_t by, int8_t bhw, int8_t bhh);

/* levels.c */
void init_level(void);
void start_new_game(void);
void decode_row(uint8_t row);
uint8_t tile_at(uint8_t tx, uint8_t ty);
uint8_t tile_solid(uint8_t tx, uint8_t ty);

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
void render_clear_tile(uint8_t tx, uint8_t ty);

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
