//
// hero.h — H.E.R.O. for Vectrex
// Shared defines, types, externs, and prototypes
//

#ifndef HERO_H
#define HERO_H

#include <vectrex.h>
#include <vectrex/stdlib.h>
#include "sprites.h"

// =========================================================================
// Constants
// =========================================================================

#define GRAVITY         1
#define THRUST          3
#define MAX_VEL_Y       4
#define MAX_VEL_X       3
#define WALK_SPEED      2
#define FUEL_DRAIN      1
#define START_FUEL      255
#define START_DYNAMITE  3
#define START_LIVES     3
#define LASER_LENGTH    40
#define LASER_LIFETIME  10
#define DYNAMITE_FUSE   60
#define EXPLOSION_TIME  20
#define EXPLOSION_RADIUS 25
#define EXPLOSION_KILL   10
#define MAX_ENEMIES     3
#define MAX_ROOMS       8
#define NONE            255

// Sprite data access: arrays are [hw, hh, oy, ox, count-1, dy1, dx1, ...]
// oy,ox = offset from logical center to VLC draw origin (at sprite scale)
#define SPRITE_HW(s)   ((s)[0])
#define SPRITE_HH(s)   ((s)[1])
#define SPRITE_OY(s)   ((s)[2])
#define SPRITE_OX(s)   ((s)[3])
#define SPRITE_VLC(s)  (&(s)[4])

// Beam intensity
#define INTENSITY_DIM    0x3F
#define INTENSITY_NORMAL 0x5F
#define INTENSITY_HI     0x6F
#define INTENSITY_BRIGHT 0x7F

// Cave geometry
#define CAVE_LEFT   -90
#define CAVE_RIGHT   90
#define CAVE_TOP     105
#define CAVE_FLOOR  -95
#define LEDGE_Y      65
#define SHAFT_LEFT  -25
#define SHAFT_RIGHT  25

// Game states
#define STATE_TITLE         0
#define STATE_PLAYING       1
#define STATE_DYING         2
#define STATE_LEVEL_COMPLETE 3
#define STATE_GAME_OVER     4

// =========================================================================
// Types
// =========================================================================

typedef struct {
    int8_t x;
    int8_t y;
    int8_t vx;
    uint8_t alive;
    uint8_t anim;
} Enemy;

// =========================================================================
// Extern globals (defined in main.c)
// =========================================================================

extern int8_t player_x;
extern int8_t player_y;
extern int8_t player_vx;
extern int8_t player_vy;
extern int8_t player_facing;
extern uint8_t player_fuel;
extern uint8_t player_dynamite;
extern uint8_t player_lives;
extern uint8_t player_on_ground;
extern uint8_t player_thrusting;
extern uint8_t anim_tick;

extern int score;

extern uint8_t game_state;
extern uint8_t current_level;
extern uint8_t current_room;
extern uint8_t death_timer;
extern uint8_t level_msg_timer;
extern const char *cur_level_name;
extern int8_t cur_level_name_x;

extern Enemy enemies[MAX_ENEMIES];
extern uint8_t enemy_count;

extern uint8_t laser_active;
extern int8_t laser_x;
extern int8_t laser_y;
extern int8_t laser_dir;
extern uint8_t laser_timer;

extern uint8_t dyn_active;
extern int8_t dyn_x;
extern int8_t dyn_y;
extern uint8_t dyn_timer;
extern uint8_t dyn_exploding;
extern uint8_t dyn_expl_timer;

extern uint8_t walls_destroyed;
extern uint8_t room_walls_destroyed[MAX_ROOMS];

extern const int8_t *cur_cave_lines;
extern int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
extern const int8_t *cur_walls;
extern uint8_t cur_wall_count;
extern const int8_t *cur_enemies_data;
extern uint8_t cur_enemy_count;
extern const int8_t *cur_cave_segs;
extern uint8_t cur_seg_count;
extern int8_t cur_miner_x;
extern int8_t cur_miner_y;
extern uint8_t cur_has_miner;

extern const int8_t *room_cave_lines[MAX_ROOMS];
extern const int8_t *room_walls[MAX_ROOMS];
extern uint8_t room_wall_counts[MAX_ROOMS];
extern const int8_t *room_enemies_data[MAX_ROOMS];
extern uint8_t room_enemy_counts[MAX_ROOMS];
extern int8_t room_starts[MAX_ROOMS * 2];
extern int8_t room_miners[MAX_ROOMS * 2];
extern uint8_t room_exits[MAX_ROOMS * 4];
extern const int8_t *room_cave_segs[MAX_ROOMS];
extern uint8_t room_seg_counts[MAX_ROOMS];
extern uint8_t room_has_miner[MAX_ROOMS];
extern int8_t room_bounds[MAX_ROOMS * 4];  // left, right, top, floor per room

extern char str_buf[16];

// =========================================================================
// Function prototypes
// =========================================================================

// main.c — helpers
uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh);
int8_t wall_y(uint8_t i);
int8_t wall_x(uint8_t i);
int8_t wall_h(uint8_t i);
int8_t wall_w(uint8_t i);
uint8_t player_hits_wall(uint8_t i);
void vectrex_init(void);

// player.c
void update_player_physics(void);
void handle_input(void);
void draw_player(void);

// enemies.c
void fire_laser(void);
void update_laser(void);
void place_dynamite(void);
void update_dynamite(void);
void update_enemies(void);
void check_miner_rescue(void);

// drawing.c
void draw_sprite(int8_t y, int8_t x, int8_t *shape);
void draw_cave(void);
void draw_enemies(void);
void draw_laser_beam(void);
void draw_dynamite_and_explosion(void);
void draw_miner(void);
void draw_hud(void);
void draw_title_screen(void);
void draw_game_over_screen(void);

// levels.c
void set_level_data(void);
void set_room_data(void);
void load_enemies(void);
void init_level(void);
void start_new_game(void);

#endif
