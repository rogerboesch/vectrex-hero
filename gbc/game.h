//
// game.h — GBC R.E.S.C.U.E. shared defines, types, externs
//
// Tilemap scrolling version: pixel coordinates, tile-based collision
//

#ifndef GAME_H
#define GAME_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <string.h>

#include "level_data.h"

#define TRUE  1
#define FALSE 0
#define NONE  255

// =========================================================================
// Game constants
// =========================================================================

#define GRAVITY         1
#define THRUST          2
#define MAX_VEL_Y       3
#define MAX_VEL_X       2
#define WALK_SPEED      1
#define FUEL_DRAIN      1
#define START_FUEL      255
#define START_DYNAMITE  3
#define START_LIVES     3
#define LASER_LENGTH    40   // pixels
#define LASER_LIFETIME  10
#define DYNAMITE_FUSE   60
#define EXPLOSION_TIME  20
#define EXPLOSION_RADIUS 24  // pixels
#define EXPLOSION_KILL   10
#define MAX_ACTIVE_ENEMIES 4
#define MAX_LEVEL_ENTITIES 64

#define ENEMY_BAT       1  // matches ENT_BAT from tilemap.h
#define ENEMY_SPIDER    2
#define ENEMY_SNAKE     3

#define SPIDER_PATROL   16  // pixels (2 tiles down from start)

// Collision boxes (pixels, half-widths/heights)
#define PLAYER_HW  3
#define PLAYER_HH  7
#define BAT_HW     5
#define BAT_HH     4
#define SPIDER_HW  4
#define SPIDER_HH  3
#define SNAKE_HW   6
#define SNAKE_HH   2
#define MINER_HW   5
#define MINER_HH   5

// Timing
#define LEVEL_INTRO_TIME     60
#define RESCUE_ANIM_TIME     60
#define DEATH_ANIM_TIME      30

// Game states
#define STATE_TITLE         0
#define STATE_PLAYING       1
#define STATE_DYING         2
#define STATE_LEVEL_COMPLETE 3
#define STATE_GAME_OVER     4
#define STATE_LEVEL_INTRO   5
#define STATE_RESCUED       6
#define STATE_LEVEL_FAILED  7

// =========================================================================
// Screen layout
// =========================================================================
//
// GBC: 160x144 px = 20x18 tiles
// Window layer: rows 0-1 = HUD (16px)
// Background: scrolling playfield (128px visible below HUD)
//
#define SCREEN_W     160
#define SCREEN_H     144
#define HUD_H         16   // 2 tile rows for HUD in background
#define PLAY_H       128   // 16 tile rows visible
#define PLAY_COLS     20
#define PLAY_ROWS     16
#define HUD_ROWS       2

// VRAM background map is 32x32 tiles (wrapping)
#define VRAM_MAP_W    32
#define VRAM_MAP_H    32

// =========================================================================
// Decode cache for tilemap rows
// =========================================================================

#define DECODE_ROWS   32   // power of 2 for fast masking
#define DECODE_MAX_W  128  // max level width + padding

// =========================================================================
// Sprite screen positioning (world px -> screen sprite pos)
// =========================================================================

// GBC hardware sprite offsets: X+8, Y+16 from top-left corner
// -4 centers 8px wide sprite on hitbox center
// -8 centers 8x16 sprite vertically on hitbox center
#define SPR_SCR_X(wpx) ((uint8_t)(8 - 4 + (int16_t)(wpx) - cam_x))
#define SPR_SCR_Y(wpy) ((uint8_t)(16 - 8 + (int16_t)(wpy) - cam_y))

// =========================================================================
// Types
// =========================================================================

typedef struct {
    int16_t px, py;       // pixel position
    int16_t home_py;      // spider home Y
    int8_t vx;
    uint8_t alive;
    uint8_t anim;
    uint8_t type;         // ENEMY_BAT, ENEMY_SPIDER, ENEMY_SNAKE
    uint8_t ent_idx;      // index into level_entities[] (to track death)
} ActiveEnemy;

typedef struct {
    uint8_t tx, ty;       // tile position (from level data)
    uint8_t type;         // entity type
    int8_t vx;            // initial velocity
    uint8_t alive;        // 0 = killed this attempt
} LevelEntity;

// =========================================================================
// Externs
// =========================================================================

// Player state (pixel coords, y-down)
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

// Camera
extern int16_t cam_x, cam_y;
extern uint8_t cam_tx, cam_ty;

// Current level info
extern uint8_t level_w, level_h;  // tiles

// Decode cache
extern uint8_t decode_cache[DECODE_ROWS][DECODE_MAX_W];

// Active enemies
extern ActiveEnemy active_enemies[MAX_ACTIVE_ENEMIES];
extern uint8_t active_enemy_count;

// Level entities
extern LevelEntity level_entities[MAX_LEVEL_ENTITIES];
extern uint8_t level_entity_count;

// Miner
extern int16_t miner_px, miner_py;
extern uint8_t miner_active;

// Laser
extern uint8_t laser_active;
extern int16_t laser_px, laser_py;
extern int8_t laser_dir;
extern uint8_t laser_timer;

// Dynamite
extern uint8_t dyn_active;
extern int16_t dyn_px, dyn_py;
extern uint8_t dyn_timer;
extern uint8_t dyn_exploding;
extern uint8_t dyn_expl_timer;

extern uint8_t joy;
extern uint8_t joy_pressed;

// =========================================================================
// Prototypes
// =========================================================================

// main.c
uint8_t box_overlap(int16_t ax, int16_t ay, int8_t ahw, int8_t ahh,
                    int16_t bx, int16_t by, int8_t bhw, int8_t bhh);

// player.c
void update_player_physics(void);
void handle_input(void);

// enemies.c
void fire_laser(void);
void update_laser(void);
void place_dynamite(void);
void update_dynamite(void);
void update_active_enemies(void);
void activate_nearby_enemies(void);
void check_miner_rescue(void);

// levels.c
void init_level(void);
void start_new_game(void);
void decode_row(uint8_t row);
uint8_t tile_at(uint8_t tx, uint8_t ty);
uint8_t tile_solid(uint8_t tx, uint8_t ty);

// tiles.c
void tiles_init(void);
void tiles_toggle_dmg(void);

// render.c
void render_init_level(void);
void render_update_camera(void);
void render_update_sprites(void);
void render_update_hud(void);
void render_hide_sprites(void);
void render_title(void);
void render_msg(const char *line1, const char *line2);
void render_clear_tile(uint8_t tx, uint8_t ty);

#endif
